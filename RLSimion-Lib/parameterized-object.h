#pragma once


class CParameters;

class CParamObject
{
protected:
	CParameters* m_pParameters;
public:
	CParamObject(CParameters* pParameters){ m_pParameters = pParameters; }
	~CParamObject(){}

	CParameters* getParameters(){ return m_pParameters; }
};