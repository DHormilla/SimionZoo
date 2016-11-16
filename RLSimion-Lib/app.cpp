#include "stdafx.h"
#include "app.h"
#include "logger.h"
#include "world.h"
#include "experiment.h"
#include "SimGod.h"
#include "config.h"
#include "globals.h"



CApp* CApp::m_pAppInstance = 0;

CApp::CApp()
{
	m_pAppInstance = this;
}

CApp::~CApp()
{
	m_pAppInstance = 0;
}

CApp* CApp::get()
{
	return m_pAppInstance;
}
