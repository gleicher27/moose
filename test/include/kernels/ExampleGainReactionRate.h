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
#ifndef EXAMPLEGAINREACTIONRATE_H
#define EXAMPLERGAINEACTIONRATE_H

#include "Reaction.h"

//Forward Declarations
class ExampleGainReactionRate;

template<>
InputParameters validParams<ExampleGainReactionRate>();

class ExampleGainReactionRate : public Reaction
{
public:
  ExampleGainReactionRate(const InputParameters & parameters);

protected:
  virtual Real computeQpResidual();
  virtual Real computeQpJacobian();

  Real _coef;
  const MaterialProperty<Real> & _mat; 
};

#endif //EXAMPLEREACTIONRATE_H
