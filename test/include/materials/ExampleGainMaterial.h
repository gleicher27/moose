/****************************************************************/
/*               DO NOT MODIFY THIS HEADER                      */
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*           (c) 2010 Battelle Energy Alliance, LLC             */
/*                   ALL RIGHTS RESERVED                        */
/*                                                              */
/*          Prepared by Battelle Energy Alliance, LLC           */
/*            Under Contract No. DE-AC07-05ID14517              */
/*            With the U. S. Department of Energy               */
/*                                                              */
/*            See COPYRIGHT for full restrictions               */
/****************************************************************/
#ifndef EXAMPLEGAINMATERIAL_H
#define EXAMPLEGAINMATERIAL_H

#include "Material.h"

//Forward Declarations
class ExampleGainMaterial;

template<>
InputParameters validParams<ExampleGainMaterial>();

/**
 * ExampleGain material class that defines a few properties.
 */
class ExampleGainMaterial : public Material
{
public:
  ExampleGainMaterial(const InputParameters & parameters);

protected:
  virtual void initQpExampleGainProperties();
  virtual void computeQpProperties();

private:
  Real _initial_gain;
  Real _gain_factor;

  /**
   * Create two MooseArray Refs to hold the current
   * and previous material properties respectively
   */
  MaterialProperty<Real> & _gain;
  MaterialProperty<Real> & _gain_old;

  const VariableValue & _u;
};

#endif //EXAMPLEGAINMATERIAL_H
