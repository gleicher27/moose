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
#ifndef EXAMPLELOSSMATERIAL_H
#define EXAMPLELOSSMATERIAL_H

#include "Material.h"

//Forward Declarations
class ExampleLossMaterial;

template<>
InputParameters validParams<ExampleLossMaterial>();

/**
 * ExampleLoss material class that defines a few properties.
 */
class ExampleLossMaterial : public Material
{
public:
  ExampleLossMaterial(const InputParameters & parameters);

protected:
  virtual void initQpExampleLossProperties();
  virtual void computeQpProperties();

private:
  Real _initial_loss;
  Real _loss_factor;

  /**
   * Create two MooseArray Refs to hold the current
   * and previous material properties respectively
   */
  MaterialProperty<Real> & _loss;
  MaterialProperty<Real> & _loss_old;

  const VariableValue & _u;
};

#endif //EXAMPLELOSSMATERIAL_H
