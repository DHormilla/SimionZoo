#include "SimGod.h"
#include "experiment.h"
#include "named-var-set.h"
#include "config.h"
#include "simion.h"
#include "app.h"
#include "deferred-load.h"
#include "utils.h"
#include "featuremap.h"
#include "experience-replay.h"
#include "parameters.h"
#include "features.h"
#include <algorithm>

std::vector<std::pair<CDeferredLoad*, unsigned int>> CSimGod::m_deferredLoadSteps;
CHILD_OBJECT_FACTORY<CStateFeatureMap> CSimGod::m_pGlobalStateFeatureMap;
CHILD_OBJECT_FACTORY<CActionFeatureMap> CSimGod::m_pGlobalActionFeatureMap;

CSimGod::CSimGod(CConfigNode* pConfigNode)
{
	if (!pConfigNode) return;

	//the global parameterizations of the state/action spaces
	m_pGlobalStateFeatureMap = CHILD_OBJECT_FACTORY<CStateFeatureMap>(pConfigNode, "State-Feature-Map", "The state feature map", true);
	m_pGlobalActionFeatureMap = CHILD_OBJECT_FACTORY<CActionFeatureMap>(pConfigNode, "Action-Feature-Map", "The state feature map", true);
	m_pExperienceReplay = CHILD_OBJECT<CExperienceReplay>(pConfigNode, "Experience-Replay", "The experience replay parameters", true);
	m_simions = MULTI_VALUE_FACTORY<CSimion>(pConfigNode, "Simion", "Simions: learning agents and controllers");

	//Gamma is global: it is considered a parameter of the problem, not the learning algorithm
	m_gamma = DOUBLE_PARAM(pConfigNode, "Gamma", "Gamma parameter", 0.9);

	m_bFreezeTargetFunctions = BOOL_PARAM(pConfigNode, "Freeze-Target-Function", "Defers updates on the V-functions to improve stability", false);
	m_targetFunctionUpdateFreq = INT_PARAM(pConfigNode, "Target-Function-Update-Freq", "Update frequency at which target functions will be updated. Only used if Freeze-Target-Function=true", 100);
	m_bUseImportanceWeights = BOOL_PARAM(pConfigNode, "Use-Importance-Weights", "Use sample importance weights to allow off-policy learning -experimental-", false);
}


CSimGod::~CSimGod()
{
}


double CSimGod::selectAction(CState* s, CAction* a)
{
	double probability = 1.0;

	for (unsigned int i = 0; i < m_simions.size(); i++)
		probability*= m_simions[i]->selectAction(s, a);

	return probability;
}

void CSimGod::update(CState* s, CAction* a, CState* s_p, double r, double probability)
{
	double actionImportanceWeight= 1.0;

	if (CSimionApp::get()->pExperiment->isEvaluationEpisode()) return;

	m_bReplayingExperience = false;

	//update step
	for (unsigned int i = 0; i < m_simions.size(); i++)
		m_simions[i]->update(s, a, s_p, r, probability);

	if (m_pExperienceReplay->bUsing())
		m_pExperienceReplay->addTuple(s, a, s_p, r, probability);
}

void CSimGod::postUpdate()
{
	CExperienceTuple* pExperienceTuple;

	//Experience Replay
	if (m_pExperienceReplay->bUsing())
	{
		m_bReplayingExperience = true;

		int updateBatchSize = m_pExperienceReplay->getUpdateBatchSize();
		for (int tuple = 0; tuple < updateBatchSize; ++tuple)
		{
			pExperienceTuple = m_pExperienceReplay->getRandomTupleFromBuffer();

			//update step
			for (unsigned int i = 0; i < m_simions.size(); i++)
				m_simions[i]->update(pExperienceTuple->s, pExperienceTuple->a, pExperienceTuple->s_p
					, pExperienceTuple->r, pExperienceTuple->probability);
		}
	}
}

void CSimGod::registerDeferredLoadStep(CDeferredLoad* deferredLoadObject, unsigned int orderLoad)
{
	m_deferredLoadSteps.push_back(std::pair<CDeferredLoad*, unsigned int>(deferredLoadObject, orderLoad));
}

bool myComparison(const std::pair<CDeferredLoad*, unsigned int> &a, const std::pair<CDeferredLoad*, unsigned int> &b)
{
	return a.second < b.second;
}

void CSimGod::deferredLoad()
{
	std::sort(m_deferredLoadSteps.begin(), m_deferredLoadSteps.end(), myComparison);

	for (auto it = m_deferredLoadSteps.begin(); it != m_deferredLoadSteps.end(); it++)
	{
		(*it).first->deferredLoadStep();
	}
}


std::shared_ptr<CStateFeatureMap> CSimGod::getGlobalStateFeatureMap()
{
	return m_pGlobalStateFeatureMap.shared_ptr();
}
std::shared_ptr<CActionFeatureMap> CSimGod::getGlobalActionFeatureMap()
{
	return m_pGlobalActionFeatureMap.shared_ptr();
}


double CSimGod::getGamma()
{
	return m_gamma.get();
}

//Returns the number of steps after which deferred V-Function updates are to be done
//0 if we don't use Freeze-V-Function
int CSimGod::getTargetFunctionUpdateFreq()
{
	if (m_bFreezeTargetFunctions.get())
		return m_targetFunctionUpdateFreq.get();
	return 0;
}

bool CSimGod::useSampleImportanceWeights()
{
	return m_bUseImportanceWeights.get();
}