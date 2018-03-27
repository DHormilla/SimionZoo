#ifdef _WIN64

#include "DQN.h"
#include "SimGod.h"
#include "worlds\world.h"
#include "app.h"
#include "featuremap.h"
#include "features.h"
#include "noise.h"
#include "deep-vfa-policy.h"
#include "parameters-numeric.h"
#include "parameters.h"
#include "../tools/CNTKWrapper/CNTKWrapper.h"
#include <algorithm>
#include "deep-vfa-policy.h"
#include "config.h"

DQN::~DQN()
{
	if (m_pMinibatchChosenActionTargetValues)
		delete[] m_pMinibatchChosenActionTargetValues;
	if (m_pMinibatchActionId)
		delete[] m_pMinibatchActionId;

	//We need to manually call the NN_DEFINITION destructor
	m_pNNDefinition.destroy();
	m_pTargetQNetwork->destroy();
	m_pOnlineQNetwork->destroy();
	CNTKWrapperLoader::UnLoad();
}

DQN::DQN(ConfigNode* pConfigNode)
{
	m_inputState = MULTI_VALUE_VARIABLE<STATE_VARIABLE>(pConfigNode, "Input-State", "Set of variables used as input of the QNetwork");
	m_numActionSteps = INT_PARAM(pConfigNode, "Num-Action-Steps", "Number of discrete values used for the output action", 2);
	m_outputAction = ACTION_VARIABLE(pConfigNode, "Output-Action", "The output action variable");

	CNTKWrapperLoader::Load();
	m_policy = CHILD_OBJECT_FACTORY<DiscreteDeepPolicy>(pConfigNode, "Policy", "The policy");
	m_pNNDefinition = NN_DEFINITION(pConfigNode, "neural-network", "Neural Network Architecture");


	m_numberOfStateVars = SimionApp::get()->pWorld->getDynamicModel()->getStateDescriptor().size();
	if (m_numberOfStateVars == 0)
		Logger::logMessage(MessageType::Error, "Invalid State-Map chosen (it will return 0 features at most, but at least 1 has to be returned)");

	if (dynamic_cast<DiscreteActionFeatureMap*>(SimGod::getGlobalActionFeatureMap().get()) == nullptr)
		Logger::logMessage(MessageType::Error, "The DiscreteEpsilonGreedyDeepPolicy requires a DiscreteActionFeatureMap as the action-feature-map.");
	
	m_pGrid = ((SingleDimensionDiscreteActionVariableGrid*)(((DiscreteActionFeatureMap*)SimGod::getGlobalActionFeatureMap().get())->returnGrid()[m_outputAction.get()]));
}

void DQN::deferredLoadStep()
{
	//heavy-weight loading and stuff that relies on the SimGod
	m_pTargetQNetwork = m_pNNDefinition.get()->createNetwork();
	m_pOnlineQNetwork = m_pTargetQNetwork->clone();

	m_numberOfActions = m_pTargetQNetwork->getTotalSize();

	//The size of the minibatch is the experience replay update size plus 1
	//This is because we only perform updates with in replaying-experience mode
	m_minibatchSize = SimionApp::get()->pSimGod->getExperienceReplayUpdateSize();

	if (m_pGrid->getNumCenters() != m_numberOfActions)
		Logger::logMessage(MessageType::Error, "Output of the network has not the same size as the discrete action grid has centers/discrete values");

	m_stateVector = vector<double>(m_numberOfStateVars, 0.0);
	m_actionValuePredictionVector = vector<double>(m_numberOfActions);

	m_minibatch_s = vector<double>(m_numberOfStateVars * m_minibatchSize, 0.0);
	m_minibatch_Q_s = vector<double>(m_numberOfActions * m_minibatchSize, 0.0);

	m_pMinibatchChosenActionTargetValues = new double[m_minibatchSize];
	m_pMinibatchActionId = new size_t[m_minibatchSize];
}

double DQN::selectAction(const State * s, Action * a)
{
	for (size_t i = 0; i < s->getNumVars(); i++)
	{
		m_stateVector[i] = s->get((int) i);
	}

	unordered_map<string, vector<double>&> inputMap =
		{ { "state-input", m_stateVector } };

	m_pOnlineQNetwork->predict(inputMap, m_actionValuePredictionVector);

	size_t resultingActionIndex = m_policy->selectAction(m_actionValuePredictionVector);

	double actionValue = m_pGrid->getCenters()[resultingActionIndex];
	a->set(m_outputAction.get(), actionValue);

	return 1.0;
}

INetwork* DQN::getPredictionNetwork()
{
	return m_pOnlineQNetwork;
}

double DQN::update(const State * s, const Action * a, const State * s_p, double r, double behaviorProb)
{
	unordered_map<string, vector<double>&> minibatchInputMap =
	{ { "state-input", m_minibatch_s } };
	size_t numInputVariables = s->getNumVars(); //TODO: FIX THIS

	vector<double> lastState = vector<double>(numInputVariables);
	vector<double> Q_s_p = vector<double>(m_numberOfActions);
	unordered_map<string, vector<double>&> lastStateInputMap =
	{ { "state-input", lastState } };
	vector<double> state = vector<double>(numInputVariables);
	vector<double> Q_s = vector<double>(m_numberOfActions);
	unordered_map<string, vector<double>&> stateInputMap =
	{ { "state-input", state } };

	if (SimionApp::get()->pSimGod->bReplayingExperience() && m_minibatchTuples<m_minibatchSize)
	{
		double gamma = SimionApp::get()->pSimGod->getGamma();

		//get Q(s_p) for the current tuple
		for (size_t i = 0; i < numInputVariables; i++)
		{
			lastState[i] = s_p->get((int)i);
		}
		//store the index of the action taken
		m_pMinibatchActionId[m_minibatchTuples] =
			m_pGrid->getClosestCenter(a->get(m_outputAction.get()));

		//estimate Q(s_p; online-weights)
		m_pOnlineQNetwork->predict(lastStateInputMap, Q_s_p);

		//calculate and store the target for the current tuple
		size_t argmaxQ = distance(Q_s_p.begin(), max_element(Q_s_p.begin(), Q_s_p.end()));

		//estimate Q(s_p, argMaxQ; target-weights or online-weights)
		//THIS is the only real difference between DQN and Double-DQN
		if (getPredictionNetwork() != m_pOnlineQNetwork)
			getPredictionNetwork()->predict(lastStateInputMap, Q_s_p);

		//calculate r + gamma*Q(s_p,a)
		m_pMinibatchChosenActionTargetValues[m_minibatchTuples] =
			r + gamma * Q_s_p[argmaxQ];

		//estimate Q(s,a)
		for (size_t i = 0; i < s->getNumVars(); i++)
		{
			state[i] = s->get((int)i);
			m_minibatch_s[m_minibatchTuples*s->getNumVars() + i] = s->get((int)i);
		}

		//get the current value of Q(s) for every action
		m_pOnlineQNetwork->predict(stateInputMap, Q_s);
		for (size_t i = 0; i < m_numberOfActions; i++)
		{
			m_minibatch_Q_s[m_minibatchTuples * m_numberOfActions + i] =
				Q_s[i];
		}

		//change only the target for the action in the tuple
		m_minibatch_Q_s
			[m_pMinibatchActionId[m_minibatchTuples] + m_minibatchTuples * m_numberOfActions] =
			m_pMinibatchChosenActionTargetValues[m_minibatchTuples];

		m_minibatchTuples++;
	}
	//We only train the network in direct-experience updates to simplify mini-batching
	else if (m_minibatchTuples == m_minibatchSize)
	{
		SimGod* pSimGod = SimionApp::get()->pSimGod.ptr();

		//update the network finally
		m_pTargetQNetwork->train(minibatchInputMap, m_minibatch_Q_s);
		m_minibatchTuples = 0;

		//update the prediction network
		if (pSimGod->bUpdateFrozenWeightsNow())
		{
			if (m_pOnlineQNetwork)
				m_pOnlineQNetwork->destroy();
			m_pOnlineQNetwork = m_pTargetQNetwork->clone();
		}
	}
	return 1.0; //TODO: Estimate the TD-error??
}


DoubleDQN::DoubleDQN(ConfigNode* pParameters): DQN (pParameters)
{}

INetwork* DoubleDQN::getPredictionNetwork()
{
	return m_pTargetQNetwork;
}

/*
#include "../tools/CNTKWrapper/CNTKLibrary.h"

//quick fix to make it compile. Will fix it later
unordered_map<CNTK::Parameter, CNTK::FunctionPtr> m_weightTransitions;


void DDPG::buildModelsParametersTransitionGraph()
{
	float tau = (float)m_tau.get();

	auto scale = CNTK::Constant::Scalar(CNTK::DataType::Float, tau, CNTK::DeviceDescriptor::CPUDevice());
	auto anitScale = CNTK::Constant::Scalar(CNTK::DataType::Float, 1.0f - tau, CNTK::DeviceDescriptor::CPUDevice());

	for (int modelIndex = 0; modelIndex <= 1; modelIndex++)
	{
		auto predictionModel = (modelIndex == 0 ? m_predictionPolicyNetwork : m_predictionQNetwork).getNetwork()->getNetworkFunctionPtr();
		auto targetModel = (modelIndex == 0 ? m_pTargetPolicyNetwork : m_pTargetQNetwork)->getNetworkFunctionPtr();

		for (int i = 0; i < predictionModel->Parameters().size(); i++)
		{
			auto targetParam = targetModel->Parameters()[i];
			auto predictionParam = predictionModel->Parameters()[i];

			auto weightTransition = CNTK::Plus(CNTK::ElementTimes(scale, predictionParam), CNTK::ElementTimes(anitScale, targetParam));
			m_weightTransitions[targetParam] = weightTransition;
		}
	}
}

//TODO: Extract in NNCreator project with wrapper functions, so that one does not have to call CNTK methods in RLSimion-Lib
void DDPG::performModelsParametersTransition()
{
	for (int modelIndex = 0; modelIndex <= 1; modelIndex++)
	{
		auto predictionModel = (modelIndex == 0 ? m_predictionPolicyNetwork : m_predictionQNetwork).getNetwork()->getNetworkFunctionPtr();
		auto targetModel = (modelIndex == 0 ? m_pTargetPolicyNetwork : m_pTargetQNetwork)->getNetworkFunctionPtr();

		for (int i = 0; i < predictionModel->Parameters().size(); i++)
		{
			auto targetParam = targetModel->Parameters()[i];
			auto weightTransition = m_weightTransitions[targetParam];

			CNTK::ValuePtr weightTransitionOutputValue;
			unordered_map<CNTK::Variable, CNTK::ValuePtr> outputs = { { weightTransition->Output(), weightTransitionOutputValue } };
			weightTransition->Evaluate({}, outputs, CNTK::DeviceDescriptor::CPUDevice());
			weightTransitionOutputValue = outputs[weightTransition->Output()];

			targetParam.SetValue(weightTransitionOutputValue->Data());
		}
	}
}

DDPG::~DDPG()
{
}

DDPG::DDPG(ConfigNode * pConfigNode)
{
	m_experienceReplay = CHILD_OBJECT<ExperienceReplay>(pConfigNode, "experience-replay", "Experience Replay", false);
	m_predictionQNetwork = NN_DEFINITION(pConfigNode, "q-neural-network", "Neural Network Architecture of Critic (Q)");
	m_predictionPolicyNetwork = NN_DEFINITION(pConfigNode, "policy-neural-network", "Neural Network Architecture of Actor (mu)");
	m_policyNoise = CHILD_OBJECT_FACTORY<Noise>(pConfigNode, "noise", "Policy Noise", false);
	m_tau = DOUBLE_PARAM(pConfigNode, "tau", "tau", 0.001);
	m_outputAction = ACTION_VARIABLE(pConfigNode, "Output-Action", "The output action variable");

	m_pStateFeatureList = new FeatureList("state-input");

	m_numberOfStateVars = SimGod::getGlobalStateFeatureMap()->getTotalNumFeatures();
	if (m_numberOfStateVars == 0)
		Logger::logMessage(MessageType::Error, "Invalid State-Map chosen (it will return 0 features at most, but at least 1 has to be returned)");

	m_pMinibatchExperienceTuples = new ExperienceTuple*[m_experienceReplay->getUpdateBatchSize()];

	m_stateVector = vector<double>(m_numberOfStateVars);
	m_actionPredictionVector = vector<double>(1);

	m_minibatch_s = vector<double>(m_numberOfStateVars * m_experienceReplay->getUpdateBatchSize(), 0.0);
	m_minibatchActionVector = vector<double>(1 * m_experienceReplay->getUpdateBatchSize(), 0.0);
	m_minibatchQVector = vector<double>(1 * m_experienceReplay->getUpdateBatchSize(), 0.0);

	m_pTargetQNetwork = m_predictionQNetwork.getNetwork()->clone();
	m_pTargetPolicyNetwork = m_predictionPolicyNetwork.getNetwork()->clone();

	buildModelsParametersTransitionGraph();
}

double DDPG::selectAction(const State * s, Action * a)
{
	//get the state vector
	SimionApp::get()->pSimGod->getGlobalStateFeatureMap()->getFeatures(s, m_pStateFeatureList);
	for (int i = 0; i < m_pStateFeatureList->m_numFeatures; i++)
		m_stateVector[m_pStateFeatureList->m_pFeatures[i].m_index] = m_pStateFeatureList->m_pFeatures[i].m_factor;

	unordered_map<string, vector<double>&> minibatchInputMap = 
		{ { "state-input", m_stateVector } };
	m_predictionPolicyNetwork.getNetwork()->predict(minibatchInputMap, m_actionPredictionVector);

	double actionValue = m_actionPredictionVector[0];
	actionValue += m_policyNoise->getSample();
	a->set(m_outputAction.get(), actionValue);

	return 1.0;
}

double DDPG::update(const State * s, const Action * a, const State * s_p, double r, double behaviorProb)
{
	double gamma = SimionApp::get()->pSimGod->getGamma();

	m_experienceReplay->addTuple(s, a, s_p, r, 1.0);

	//don't update anything if the experience replay buffer does not contain enough elements for at least one minibatch
	if (m_experienceReplay->getUpdateBatchSize() != m_experienceReplay->getUpdateBatchSize())
		return 0.0;

	for (int i = 0; i < m_experienceReplay->getUpdateBatchSize(); i++)
	{
		m_pMinibatchExperienceTuples[i] = m_experienceReplay->getRandomTupleFromBuffer();
	}

	updateCritic();
	updateActor();

	performModelsParametersTransition();

	return 0.0;
}

void DDPG::updateActor()
{
	CNTK::FunctionPtr modelPolicyOutputPtr = m_predictionPolicyNetwork.getNetwork()->getOutputsFunctionPtr()[0];
	CNTK::FunctionPtr modelQOutputPtr = m_predictionQNetwork.getNetwork()->getOutputsFunctionPtr()[0];

	for (int i = 0; i < m_experienceReplay->getUpdateBatchSize(); i++)
	{
		//get Q(s_p) for entire minibatch
		SimionApp::get()->pSimGod->getGlobalStateFeatureMap()->getFeatures(m_pMinibatchExperienceTuples[i]->s, m_pStateFeatureList);
		for (int n = 0; n < m_pStateFeatureList->m_numFeatures; n++)
			m_minibatch_s[m_pStateFeatureList->m_pFeatures[n].m_index + i * m_numberOfStateVars] = m_pStateFeatureList->m_pFeatures[n].m_factor;

		m_minibatchActionVector[i] = m_pMinibatchExperienceTuples[i]->a->get(m_outputAction.get());
	}

	CNTK::ValuePtr stateInputValue = CNTK::Value::CreateBatch(m_predictionQNetwork.getNetwork()->getInputs()[0]->getInputVariable().Shape(), m_minibatch_s, CNTK::DeviceDescriptor::CPUDevice());

	//get gradient of Q
	CNTK::ValuePtr chosenActionValuePtr;
	unordered_map<CNTK::Variable, CNTK::ValuePtr> getChoosenActionInputMap =
	{
		{ m_predictionPolicyNetwork.getNetwork()->getInputs()[0]->getInputVariable()
		, stateInputValue }
	};

	unordered_map<CNTK::Variable, CNTK::ValuePtr> chosenActionOutputMap = { { modelPolicyOutputPtr, chosenActionValuePtr } };
	m_predictionPolicyNetwork.getNetwork()->getNetworkFunctionPtr()->Evaluate(getChoosenActionInputMap, chosenActionOutputMap, CNTK::DeviceDescriptor::CPUDevice());
	chosenActionValuePtr = chosenActionOutputMap[modelPolicyOutputPtr];

	unordered_map<CNTK::Variable, CNTK::ValuePtr> qGradientActionArgument = { { m_predictionQNetwork.getNetwork()->getInputs()[0]->getInputVariable(), stateInputValue } ,
		{ m_predictionQNetwork.getNetwork()->getInputs()[1]->getInputVariable(), chosenActionValuePtr } };
	unordered_map<CNTK::Variable, CNTK::ValuePtr> qGradientOutputMap =
		{ { m_predictionQNetwork.getNetwork()->getInputs()[1]->getInputVariable(), nullptr } };
	modelQOutputPtr->Gradients(qGradientActionArgument, qGradientOutputMap, CNTK::DeviceDescriptor::CPUDevice());

	//get the gradient of mu/the policy
	//forward propagation of values
	//therefore, fill up the input variables
	unordered_map<CNTK::Variable, CNTK::ValuePtr> policyInputMap;
	policyInputMap[m_predictionPolicyNetwork.getNetwork()->getInputs()[0]->getInputVariable()] = stateInputValue;

	unordered_map<CNTK::Variable, CNTK::ValuePtr> policyOutputMap;
	policyOutputMap[m_predictionPolicyNetwork.getNetwork()->getOutputsFunctionPtr()[0]->Output()] = nullptr;

	//perform now the forward propagation
	auto backPropState = modelPolicyOutputPtr->Forward(policyInputMap, policyOutputMap, CNTK::DeviceDescriptor::CPUDevice(), { modelPolicyOutputPtr });
	CNTK::ValuePtr policyOutputValue = policyOutputMap[m_predictionPolicyNetwork.getNetwork()->getOutputsFunctionPtr()[0]->Output()];

	CNTK::ValuePtr rootGradientValue = qGradientOutputMap[m_predictionQNetwork.getNetwork()->getInputs()[1]->getInputVariable()];

	unordered_map<CNTK::Variable, CNTK::ValuePtr> parameterGradients;
	for (const auto& parameter : modelPolicyOutputPtr->Parameters())
		//This is not working: set the gradients to the Q gradient to multipy them with the current value
		//therefore, use nullptr
		parameterGradients[parameter] = CNTK::ValuePtr();
	//calculate grad_q * grad_policy for each parameter's weight
	modelPolicyOutputPtr->Backward(backPropState, { { modelPolicyOutputPtr, rootGradientValue } }, parameterGradients);

	//update the parameters of the policy now
	unordered_map<CNTK::Parameter, CNTK::NDArrayViewPtr> gradients;
	for (const auto& parameter : modelPolicyOutputPtr->Parameters())
		gradients[parameter] = parameterGradients[parameter]->Data();

	m_predictionPolicyNetwork.getNetwork()->getTrainer()->ParameterLearners()[0]->Update(gradients, m_experienceReplay->getUpdateBatchSize());

	
	//we have to perform something like this

	//self.q_gradient_input = tf.placeholder("float",[None,self.action_dim])
	//self.parameters_gradients = tf.gradients(self.action_output,self.net,-self.q_gradient_input)
	//self.optimizer = tf.train.AdamOptimizer(LEARNING_RATE).apply_gradients(zip(self.parameters_gradients,self.net))

	//maybe this helps
	//https://stackoverflow.com/questions/41814858/how-to-access-gradients-and-modify-weights-parameters-directly-during-training

	//python: Learner.update(gradient_values, training_sample_count)
	//C++:    model->getTrainer()->ParameterLearners()[0]->Update();
	
}

void DDPG::updateCritic()
{
	double gamma = SimionApp::get()->pSimGod->getGamma();

	for (int i = 0; i < m_experienceReplay->getUpdateBatchSize(); i++)
	{
		SimionApp::get()->pSimGod->getGlobalStateFeatureMap()->getFeatures(m_pMinibatchExperienceTuples[i]->s_p, m_pStateFeatureList);
		for (int n = 0; n < m_pStateFeatureList->m_numFeatures; n++)
			m_minibatch_s[m_pStateFeatureList->m_pFeatures[n].m_index + i * m_numberOfStateVars] = m_pStateFeatureList->m_pFeatures[n].m_factor;
	}

	//calculate mu'(s_p)
	unordered_map<string, vector<double>&> minibatchInputMap =
		{ { "state-input", m_minibatch_s } };
	m_pTargetPolicyNetwork->predict(minibatchInputMap, m_minibatchActionVector);

	//calculate Q'(mu'(s_p)) now
	minibatchInputMap = 
		{ 
			{ "state-input", m_minibatch_s }
			,{ "action-input", m_minibatchActionVector } 
		};
	m_pTargetQNetwork->predict(minibatchInputMap, m_minibatchQVector);

	//calculate y_i
	for (int i = 0; i < m_experienceReplay->getUpdateBatchSize(); i++)
	{
		m_minibatchQVector[i] = m_pMinibatchExperienceTuples[i]->r + gamma * m_minibatchQVector[i];
	}

	//update the network finally
	for (int i = 0; i < m_experienceReplay->getUpdateBatchSize(); i++)
	{
		m_minibatchActionVector[i] = m_pMinibatchExperienceTuples[i]->a->get(m_outputAction.get());
	}

	minibatchInputMap = 
	{ 
		{ "state-input", m_minibatch_s }
		,{ "action-input", m_minibatchActionVector }
	};

	m_predictionQNetwork.getNetwork()->train(minibatchInputMap, m_minibatchQVector);
}
*/
#endif