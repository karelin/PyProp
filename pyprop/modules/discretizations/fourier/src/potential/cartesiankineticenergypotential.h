#ifndef CARTESIANKINETICENERGYPOTENTIAL_H
#define CARTESIANKINETICENERGYPOTENTIAL_H

#include <core/potential/dynamicpotentialevaluator.h>

/* 
Dynamic potential for evaluation of the kinetic energy potential for CartesianFFTEvaluator
*/
template<int Rank>
class CartesianKineticEnergyPotential : public PotentialBase<Rank>
{
public:
	double Mass;

	void ApplyConfigSection(const ConfigSection &config)
	{
		config.Get("mass", Mass);
	}

	inline double GetPotentialValue(const blitz::TinyVector<double, Rank> &momentum)
	{
		double kineticPotential = 0.0;
		for (int i=0;i<Rank;i++)
		{
			kineticPotential += sqr(momentum(i));
		}
		return kineticPotential/(2.0 * Mass);
	}
};


/*
 * Radial Kinetic Energy potential for the reduced wavefunction
 */
template<int Rank>
class RadialKineticEnergyPotential : public PotentialBase<Rank>
{
public:
	double Mass;
	int RadialRank;

	void ApplyConfigSection(const ConfigSection &config)
	{
		config.Get("mass", Mass);
		config.Get("radial_rank", RadialRank);
	}

	inline double GetPotentialValue(const blitz::TinyVector<double, Rank> &pos )
	{
		return sqr(pos(RadialRank))/(2*Mass);
	}
};


#endif
