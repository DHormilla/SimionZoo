#include "stdafx.h"
#include "experiment.h"
#include "parameters.h"
#include "logger.h"
#include "world.h"
#include "globals.h"
#include "stats.h"



CExperimentTime& CExperimentTime::operator=(CExperimentTime& exp)
{
	m_step = exp.m_step;
	m_episodeIndex = exp.m_episodeIndex;

	return *this;
}

bool CExperimentTime::operator==(CExperimentTime& exp)
{
	return m_step == exp.m_step && m_episodeIndex == exp.m_episodeIndex;
}

double CExperiment::getExperimentProgress()
{
	double progress = ((double)m_step - 1 + (m_episodeIndex - 1)*m_numSteps)
		/ ((double)m_numSteps*m_totalNumEpisodes - 1);
	return progress;
}

double CExperiment::getEpisodeProgress()
{
	double progress = ((double)m_step - 1)	/ ((double)m_numSteps - 1);
	return progress;
}

const char* CExperiment::getProgressString()
{
	sprintf_s(m_progressMsg, MAX_PROGRESS_MSG_LEN, "Episode: %d/%d Step %d/%d (%.2f%%)"
		, getEpisodeIndex(), getTotalNumEpisodes()
		, getStep(), getNumSteps()
		, getExperimentProgress()*100.0);
	return m_progressMsg;
}


bool CExperiment::isEvaluationEpisode()
{
	if (m_evalFreq > 0)
	{
		return (m_episodeIndex - 1) % (m_evalFreq + 1) == 0;
	}
	return false;
}

void CExperiment::reset()
{
	m_trainingEpisodeIndex= 0; //[1..m_numTrainingEpisodes]
	m_evalEpisodeIndex= 0; //[1..1+m_numTrainingEpisodes/ evalFreq]
	m_episodeIndex= 0; //[1..g_numEpisodes]
	m_step= 0; //]1..g_numStepsPerEpisode]
}


void CExperiment::nextStep()
{
	if (m_step<m_numSteps)
		m_step ++;

	else m_step = 0;
}

void CExperiment::nextEpisode()
{
	m_episodeIndex++;
	if (isEvaluationEpisode()) m_evalEpisodeIndex++;
	else m_trainingEpisodeIndex++;
}

bool CExperiment::isFirstEpisode()
{
	if (isEvaluationEpisode()) return m_evalEpisodeIndex == 1;
	return m_trainingEpisodeIndex == 1;
}

bool CExperiment::isLastEpisode()
{
	if (isEvaluationEpisode()) return m_evalEpisodeIndex == m_numEvaluationEpisodes;
	return m_trainingEpisodeIndex == m_numTrainingEpisodes;
}

CExperiment::~CExperiment()
{
}

CExperiment::CExperiment(CParameters* pParameters)
{
	if (pParameters)
	{
		m_progUpdateFreq = pParameters->getConstDouble("Progress-Update-Freq", 0.5);
		m_randomSeed = pParameters->getConstInteger("Random-Seed", 1);


		m_numTrainingEpisodes = pParameters->getConstInteger("Num-Episodes", 1);
		m_evalFreq = std::min((int)m_numTrainingEpisodes, pParameters->getConstInteger("Eval-Freq", 0));
		if (m_evalFreq != 0)
		{
			m_numEvaluationEpisodes = 1 + m_numTrainingEpisodes / m_evalFreq;
			m_totalNumEpisodes = m_numTrainingEpisodes + m_numEvaluationEpisodes;
		}
		else
		{
			m_numEvaluationEpisodes = 0;
			m_totalNumEpisodes = m_numTrainingEpisodes;
		}
		setNumSteps((unsigned int)(pParameters->getConstDouble("Episode-Length", 1.0) / RLSimion::g_pWorld->getDT()));
		reset();

		m_pLogger = new CLogger(pParameters->getChild("Log"));
	}
	else
	{
		m_randomSeed = 0;
		m_numEvaluationEpisodes = 0;
		m_totalNumEpisodes = 0;
		m_numTrainingEpisodes = 0;
		m_evalFreq = 0;
		setNumSteps(0);
	}
	QueryPerformanceFrequency((LARGE_INTEGER*)&m_counterFreq);
	srand(m_randomSeed);
}




void CExperiment::timestep(CState* s, CAction* a, CState* s_p, CReward* r)
{
	//print progress
	__int64 currentCounter;
	QueryPerformanceCounter((LARGE_INTEGER*)&currentCounter);
	double time = (double)(currentCounter - m_lastProgressReportCounter) / (double)m_counterFreq;

	if (time>m_progUpdateFreq)
	{
		CLogger::logMessage(Progress, RLSimion::g_pExperiment->getProgressString());
		m_lastProgressReportCounter = currentCounter;
	}

	bool evalEpisode = isEvaluationEpisode();
	if (isFirstEpisode() && isFirstStep())
		m_pLogger->firstEpisode(evalEpisode);

	unsigned int episodeIndex = getRelativeEpisodeIndex();
	if (isFirstStep())
		m_pLogger->firstStep(evalEpisode, episodeIndex);

	//update stats
	//output step-stats
	m_pLogger->timestep(evalEpisode, episodeIndex);

	if (isLastStep())
		m_pLogger->lastStep(evalEpisode, episodeIndex);

	if (isLastEpisode() && isLastStep())
		m_pLogger->lastEpisode(evalEpisode);
}