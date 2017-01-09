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
#include "ExampleGainReactionRate.h"

template<>
InputParameters validParams<ExampleGainReactionRate>()
{
  InputParameters params = validParams<Reaction>();
  params.addParam<Real>("coefficient", 1.0, "Coefficient of the term");
  return params;
}

ExampleGainReactionRate::ExampleGainReactionRate(const InputParameters & parameters) :
    Reaction(parameters),
    _coef(getParam<Real>("coefficient")),
    _mat(getMaterialProperty<Real>("examplegainmaterial"))
{
}

Real
ExampleGainReactionRate::computeQpResidual()
{
  return _coef * _mat[_qp] * Reaction::computeQpResidual();
}

Real
ExampleGainReactionRate::computeQpJacobian()
{
  return _coef * _mat[_qp] * Reaction::computeQpJacobian();
}
