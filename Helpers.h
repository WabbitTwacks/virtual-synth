#pragma once

#include <cmath>

double LinToLog(double linValue, double linStart, double linEnd, double logStart, double logEnd) //Returns a corresponding logarithmic value mapped from a linear scale to a logarithmic scale
{
	double b = log10(logEnd / logStart) / (linEnd - linStart);
	double a = logEnd / pow(10, b*linEnd);
	double logValue = a * pow(10, b * linValue);

	return logValue;
}

double LogToLin(double logValue, double logStart, double logEnd, double linStart, double linEnd) //Returns a corresponding linear value mapped from a logarithmic scale to a linear scale
{
	double a = (linEnd - linStart) / log10(logEnd / logStart);
	double b = linEnd - log10(logEnd)*a;

	double linValue = a * log10(logValue) + b;

	return linValue;
}