#pragma once

#include <cmath>

double LinToLog(double linValue, double linStart, double linEnd, double logStart, double logEnd) //Returns a corresponding logarithmic value mapped from a linear scale to a logarithmic scale
{
	double b = log(logEnd / logStart) / (linEnd - linStart);
	double a = logEnd / exp(b*linEnd);
	double logValue = a * exp(b * linValue);

	return logValue;
}

double LogToLin(double logValue, double logStart, double logEnd, double linStart, double linEnd) //Returns a corresponding linear value mapped from a logarithmic scale to a linear scale
{
	//TODO: implement

	return 0.0;
}