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
#include "ExampleLossMaterial.h"

template<>
InputParameters validParams<ExampleLossMaterial>()
{
  InputParameters params = validParams<Material>();
  params.addParam<Real>("initial_loss", 0.5, "The Initial Loss");
  params.addParam<Real>("loss_factor",0.0,"The loss factor");
  params.addCoupledVar("variable","The variable hooked into this material");
  return params;
}

ExampleLossMaterial::ExampleLossMaterial(const InputParameters & parameters) :
    Material(parameters),

    // Get a parameter value for the examplelossmaterial
    _initial_loss(getParam<Real>("initial_loss")),
    _loss_factor(getParam<Real>("loss_factor")),

    // Declare that this material is going to have a Real
    // valued property named "examplelossmaterial" that Kernels can use.
    _loss(declareProperty<Real>("examplelossmaterial")),

    // Declare that we are going to have an old value of examplelossmaterial
    // Note: this is _expensive_ and currently means that you can't
    // use adaptivity!  Only do this if you REALLY need it!
    _loss_old(declarePropertyOld<Real>("examplelossmaterial")),
    _u(coupledValue("variable"))
{
}

void
ExampleLossMaterial::initQpExampleLossProperties()
{
  _loss[_qp] = _initial_loss;
}

void
ExampleLossMaterial::computeQpProperties()
{
  _loss[_qp] =  _loss_factor*_u[_qp];
}
