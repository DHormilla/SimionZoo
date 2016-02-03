#include "stdafx.h"
#include "SimGod.h"
#include "parameters.h"
#include "parameter.h"
#include "critic.h"
#include "actor.h"


CSimGod::CSimGod(CParameters* pParameters)
{
	CParameters* child = pParameters->getChild(0);

	if (strcmp(child->getName(), "ACTOR-CRITIC") == 0)
	{
		m_pController = CActor::getInstance(child->getChild("ACTOR"));
		m_pActor = m_pController;
		m_pCritic = CCritic::getInstance(child->getChild("CRITIC"));
	}
	else if (strcmp(child->getName(), "ACTOR-CRITIC-CONTROLLER") == 0)
	{
		m_pController = CActor::getInstance(child->getChild("CONTROLLER")); //CActor is a singleton
		m_pActor = CActor::getInstance(child->getChild("ACTOR"));
		m_pCritic = CCritic::getInstance(child->getChild("CRITIC"));
	}
	else if (strcmp(child->getName(), "ACTOR-ONLY") == 0)
	{
		m_pController = CActor::getInstance(child->getChild("ACTOR"));
		m_pActor = m_pController;
		m_pCritic = 0;
	}
	m_rho = 0.0;
	
}


CSimGod::~CSimGod()
{
	if (m_pCritic) delete m_pCritic;
	if (m_pController && m_pController != m_pActor) delete m_pController;
	if (m_pActor) delete m_pActor;
}


double CSimGod::selectAction(CState* s, CAction* a)
{

	//the controller selects the action: might be the actor
	m_pController->selectAction(s, a);

	if (m_pController == m_pActor)
		m_rho = 1.0;
	else
	{
		double controllerProb = m_pController->getProbability(s, a);
		double actorProb = m_pActor->getProbability(s, a);
		m_rho = actorProb / controllerProb;
	}
		
	return m_rho;
}

double CSimGod::update(CState* s, CAction* a, CState* s_p, double r)
{
	double td= 0.0;

	//update critic
	if (m_pCritic)
		td = m_pCritic->updateValue(s, a, s_p, r, m_rho);

	//update actor: might be the controller
	m_pActor->updatePolicy(s, a, s_p, r, td);

	return td;
}