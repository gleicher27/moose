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
#include "ExampleGainMaterial.h"

template<>
InputParameters validParams<ExampleGainMaterial>()
{
  InputParameters params = validParams<Material>();
  params.addParam<Real>("initial_gain", 0.5, "The Initial Gain in the material");
  params.addParam<Real>("gain_factor",0.0,"The gain factor to help control gowth");
  params.addCoupledVar("variable","The variable hooked into this material");
  return params;
}

ExampleGainMaterial::ExampleGainMaterial(const InputParameters & parameters) :
    Material(parameters),

    // Get a parameter value for the diffusivity
    _initial_gain(getParam<Real>("initial_gain")),
    _gain_factor(getParam<Real>("gain_factor")),

    // Declare that this material is going to have a Real
    // valued property named "diffusivity" that Kernels can use.
    _gain(declareProperty<Real>("examplegainmaterial")),

    // Declare that we are going to have an old value of diffusivity
    // Note: this is _expensive_ and currently means that you can't
    // use adaptivity!  Only do this if you REALLY need it!
    _gain_old(declarePropertyOld<Real>("examplegainmaterial")),
    _u(coupledValue("variable"))
{
}

void
ExampleGainMaterial::initQpExampleGainProperties()
{
  _gain[_qp] = _initial_gain;
}

void
ExampleGainMaterial::computeQpProperties()
{
  _gain[_qp] =  - _gain_factor*_u[_qp];
}
