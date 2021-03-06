/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/
#include "Conversion.h"
#include "FEProblem.h"
#include "Factory.h"
#include "MooseMesh.h"
#include "MooseObjectAction.h"
#include "TensorMechanicsAction.h"

#include "libmesh/string_to_enum.h"
#include <algorithm>

// map tensor name shortcuts to tensor material property names
const std::map<std::string, std::string> TensorMechanicsAction::_ranktwoaux_table = {
    {"strain", "total_strain"},
    {"stress", "stress"},
    {"elastic_strain", "elastic_strain"},
    {"plastic_strain", "plastic_strain"},
    {"creep_strain", "creep_strain"}};
const std::vector<char> TensorMechanicsAction::_component_table = {'x', 'y', 'z'};
// map aux variable name prefixes to RanTwoScalarAux option and list of permitted tensor name shortcuts
const std::map<std::string, std::pair<std::string, std::vector<std::string>>> TensorMechanicsAction::_ranktwoscalaraux_table = {
    {"vonmises", {"VonMisesStress", {"stress"}}},
    {"hydrostatic", {"Hydrostatic", {"stress"}}},
    {"max_principal", {"MaxPrincipal", {"stress"}}},
    {"mid_principal", {"MidPrincipal", {"stress"}}},
    {"min_principal", {"MinPrincipal", {"stress"}}},
    {"equivalent", {"EquivalentPlasticStrain", {"plastic_strain", "creep_strain"}}},
    {"firstinv", {"FirstInvariant", {"stress", "strain"}}},
    {"secondinv", {"SecondInvariant", {"stress", "strain"}}},
    {"thirdinv", {"ThirdInvariant", {"stress", "strain"}}}};

template <>
InputParameters
validParams<TensorMechanicsAction>()
{
  InputParameters params = validParams<Action>();
  params.addClassDescription("Set up stress divergence kernels with coordinate system aware logic");
  params.addRequiredParam<std::vector<NonlinearVariableName>>("displacements", "The nonlinear displacement variables for the problem");
  params.addParam<NonlinearVariableName>("temp", "The temperature"); // Deprecated
  params.addParam<NonlinearVariableName>("temperature", "The temperature");

  MooseEnum strainType("SMALL FINITE", "SMALL");
  params.addParam<MooseEnum>("strain", strainType, "Strain formulation");
  params.addParam<bool>("incremental", "Use incremental or total strain");

  MooseEnum planarFormulationType("NONE PLANE_STRESS PLANE_STRAIN GENERALIZED_PLANE_STRAIN", "NONE");
  params.addParam<MooseEnum>("planar_formulation", planarFormulationType, "Out-of-plane stress/strain formulation");

  params.addParam<std::string>("base_name", "Material property base name");
  params.addParam<bool>("volumetric_locking_correction", false, "Flag to correct volumetric locking");
  params.addParam<bool>("use_finite_deform_jacobian", false, "Jacobian for corrotational finite strain");
  params.addParam<bool>("use_displaced_mesh", false, "Whether to use displaced mesh in the kernels");
  params.addParam<bool>("add_variables", false, "Add the displacement variables");
  params.addParam<std::vector<MaterialPropertyName>>("eigenstrain_names", "List of eigenstrains to be applied in this strain calculation");

  // Advanced
  params.addParam<std::vector<SubdomainName>>("block", "The list of ids of the blocks (subdomain) that the stress divergence kernels will be applied to");
  params.addParam<std::vector<AuxVariableName>>("save_in", "The displacement residuals");
  params.addParam<std::vector<AuxVariableName>>("diag_save_in", "The displacement diagonal preconditioner terms");
  params.addParamNamesToGroup("block save_in diag_save_in", "Advanced");

  // Output
  std::string options = "";
  for (auto & r2a : TensorMechanicsAction::_ranktwoaux_table)
    for (unsigned int a = 0; a < 3; ++a)
      for (unsigned int b = 0; b < 3; ++b)
        options += (options == "" ? "" : " ") + r2a.first + '_' + TensorMechanicsAction::_component_table[a] + TensorMechanicsAction::_component_table[b];

  for (auto & r2sa : TensorMechanicsAction::_ranktwoscalaraux_table)
    for (auto & t : r2sa.second.second)
      options += " " + r2sa.first + "_" + t;

  MultiMooseEnum outputPropertiesType(options);
  params.addParam<MultiMooseEnum>("generate_output", outputPropertiesType, "Add scalar quantity output for stress and/or strain");

  return params;
}

TensorMechanicsAction::TensorMechanicsAction(const InputParameters & params)
  : Action(params),
    _displacements(getParam<std::vector<NonlinearVariableName>>("displacements")),
    _ndisp(_displacements.size()),
    _coupled_displacements(_ndisp),
    _save_in(getParam<std::vector<AuxVariableName>>("save_in")),
    _diag_save_in(getParam<std::vector<AuxVariableName>>("diag_save_in")),
    _subdomain_names(getParam<std::vector<SubdomainName>>("block")),
    _subdomain_ids(),
    _strain(getParam<MooseEnum>("strain").getEnum<Strain>()),
    _planar_formulation(getParam<MooseEnum>("planar_formulation").getEnum<PlanarFormulation>()),
    _eigenstrain_names(getParam<std::vector<MaterialPropertyName>>("eigenstrain_names"))
{
  // determine if incremental strains are to be used
  if (isParamValid("incremental"))
  {
    const bool incremental = getParam<bool>("incremental");
    if (!incremental && _strain == Strain::Small)
      _strain_and_increment = StrainAndIncrement::SmallTotal;
    else if (!incremental && _strain == Strain::Finite)
      _strain_and_increment = StrainAndIncrement::FiniteTotal;
    else if (incremental && _strain == Strain::Small)
      _strain_and_increment = StrainAndIncrement::SmallIncremental;
    else if (incremental && _strain == Strain::Finite)
      _strain_and_increment = StrainAndIncrement::FiniteIncremental;
    else
      mooseError("Internal error");
  }
  else
  {
    if (_strain == Strain::Small)
      _strain_and_increment = StrainAndIncrement::SmallTotal;
    else if (_strain == Strain::Finite)
      _strain_and_increment = StrainAndIncrement::FiniteIncremental;
    else
      mooseError("Internal error");
  }

  // determine if displaced mesh is to be used
  _use_displaced_mesh = (_strain == Strain::Finite);
  if (params.isParamSetByUser("use_displaced_mesh"))
  {
    bool use_displaced_mesh_param = getParam<bool>("use_displaced_mesh");
    if (use_displaced_mesh_param != _use_displaced_mesh && params.isParamSetByUser("strain"))
      mooseError("Wrong combination of use displaced mesh and strain model");
    _use_displaced_mesh = use_displaced_mesh_param;
  }

  // convert vector of NonlinearVariableName to vector of VariableName
  for (unsigned int i = 0; i < _ndisp; ++i)
    _coupled_displacements[i] = _displacements[i];

  if (_save_in.size() != 0 && _save_in.size() != _ndisp)
    mooseError("Number of save_in variables should equal to the number of displacement variables " << _ndisp);

  if (_diag_save_in.size() != 0 && _diag_save_in.size() != _ndisp)
    mooseError("Number of diag_save_in variables should equal to the number of displacement variables " << _ndisp);

  // plane strain consistency check
  if (_planar_formulation != PlanarFormulation::None && _ndisp != 2)
    mooseError("Plane strain only works in 2 dimensions");

  if (_planar_formulation != PlanarFormulation::None)
    mooseError("Not implemented");

  // convert output vareiable names to lower case
  for (const auto & out : getParam<MultiMooseEnum>("generate_output"))
  {
    std::string lower(out);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    _generate_output.push_back(lower);
  }

  // Error if volumetric locking correction is true for 1D problems
  if (_ndisp == 1 && getParam<bool>("volumetric_locking_correction"))
    mooseError("Volumetric locking correction should be set to false for 1D problems.");
}

void
TensorMechanicsAction::act()
{
  //
  // Consistency check for the coordinate system
  //
  actSubdomainChecks();

  //
  // Gather info from all other TensorMechanicsAction
  //
  actGatherActionParameters();

  //
  // Deal with the optional AuxVariable based tensor quantity output
  //
  actOutputGeneration();

  //
  // Meta action which optionally spawns other actions
  //
  if (_current_task == "meta_action")
  {
    const std::string type = "GeneralizedPlaneStrainAction";
    auto params = _action_factory.getValidParams(type);

    if (_planar_formulation == PlanarFormulation::GeneralizedPlaneStrain)
    {
      auto action = MooseSharedNamespace::static_pointer_cast<MooseObjectAction>(_action_factory.create("type", name() + "_gps", params));

      // Set the object parameters
      InputParameters & object_params = action->getObjectParams();
      object_params.set<bool>("_built_by_moose") = true;

      // Add the action to the warehouse
      _awh.addActionBlock(action);
    }
  }

  //
  // Add variables (optional)
  //
  else if (_current_task == "add_variable" && getParam<bool>("add_variables"))
  {
    // determine necessary order
    const bool second = _problem->mesh().hasSecondOrderElements();

    // Loop through the displacement variables
    for (const auto & disp : _displacements)
    {
      // Create displacement variables
      _problem->addVariable(disp,
                            FEType(Utility::string_to_enum<Order>(second ? "SECOND" : "FIRST"),
                                   Utility::string_to_enum<FEFamily>("LAGRANGE")),
                            1.0, _subdomain_id_union.empty() ? nullptr : &_subdomain_id_union);
    }
  }

  //
  // Add Stess and Strain Materials (optional)
  //
  else if (_current_task == "add_material")
  {
    std::string type;

    //
    // no plane strain
    //
    if (_planar_formulation == PlanarFormulation::None)
    {
      std::map<std::pair<Moose::CoordinateSystemType, StrainAndIncrement>, std::string> type_map = {
          {{Moose::COORD_XYZ, StrainAndIncrement::SmallTotal}, "ComputeSmallStrain"},
          {{Moose::COORD_XYZ, StrainAndIncrement::SmallIncremental}, "ComputeIncrementalSmallStrain"},
          {{Moose::COORD_XYZ, StrainAndIncrement::FiniteIncremental}, "ComputeFiniteStrain"},
          {{Moose::COORD_RZ, StrainAndIncrement::SmallTotal}, "ComputeAxisymmetricRZSmallStrain"},
          {{Moose::COORD_RZ, StrainAndIncrement::SmallIncremental}, "ComputeAxisymmetricRZIncrementalStrain"},
          {{Moose::COORD_RZ, StrainAndIncrement::FiniteIncremental}, "ComputeAxisymmetricRZFiniteStrain"},
          {{Moose::COORD_RSPHERICAL, StrainAndIncrement::SmallTotal}, "ComputeRSphericalSmallStrain"},
          {{Moose::COORD_RSPHERICAL, StrainAndIncrement::SmallIncremental}, "ComputeRSphericalIncrementalStrain"},
          {{Moose::COORD_RSPHERICAL, StrainAndIncrement::FiniteIncremental}, "ComputeRSphericalFiniteStrain"}};

      auto type_it = type_map.find(std::make_pair(_coord_system, _strain_and_increment));
      if (type_it != type_map.end())
        type = type_it->second;
      else
        mooseError("Unsupported strain formulation");
    }
    else
    {
      std::map<StrainAndIncrement, std::string> type_map = {
          {StrainAndIncrement::SmallTotal, "ComputePlaneSmallStrain"},
          {StrainAndIncrement::SmallIncremental, "ComputePlaneIncrementalStrain"},
          {StrainAndIncrement::FiniteIncremental, "ComputePlaneFiniteStrain"}};

      // choose kernel type based on coordinate system
      auto type_it = type_map.find(_strain_and_increment);
      if (type_it != type_map.end())
        type = type_it->second;
      else
        mooseError("Unsupported coordinate system for plane strain.");
    }

    // set material parameters
    auto params = _factory.getValidParams(type);
    params.applyParameters(parameters(), {"displacements", "use_displaced_mesh", "eigenstrain_names"});

    params.set<std::vector<VariableName>>("displacements") = _coupled_displacements;
    params.set<bool>("use_displaced_mesh") = false;
    params.set<std::vector<MaterialPropertyName>>("eigenstrain_names") = _eigenstrain_names;

    _problem->addMaterial(type, name() + "_strain", params);
  }

  //
  // Add Stress Divergence Kernels
  //
  else if (_current_task == "add_kernel")
  {
    auto tensor_kernel_type = getKernelType();
    auto params = getKernelParameters(tensor_kernel_type);

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      std::string kernel_name = "TM_" + name() + Moose::stringify(i);

      params.set<unsigned int>("component") = i;
      params.set<NonlinearVariableName>("variable") = _displacements[i];

      if (_save_in.size() == _ndisp)
        params.set<std::vector<AuxVariableName>>("save_in") = {_save_in[i]};
      if (_diag_save_in.size() == _ndisp)
        params.set<std::vector<AuxVariableName>>("diag_save_in") = {_diag_save_in[i]};

      _problem->addKernel(tensor_kernel_type, kernel_name, params);
    }
  }
}

void
TensorMechanicsAction::actSubdomainChecks()
{
  //
  // Do the coordinate system check only once the problem is created
  //
  if (_current_task == "setup_mesh_complete")
  {
    // get subdomain IDs
    for (auto & name : _subdomain_names)
      _subdomain_ids.insert(_mesh->getSubdomainID(name));
  }

  if (_current_task == "validate_coordinate_systems")
  {
    // use either block restriction list or list of all subdomains in the mesh
    const auto & check_subdomains = _subdomain_ids.empty() ? _problem->mesh().meshSubdomains() : _subdomain_ids;
    if (check_subdomains.empty())
      mooseError("No subdomains found");

    // make sure all subdomains are using the same coordinate system
    _coord_system = _problem->getCoordSystem(*check_subdomains.begin());
    for (auto subdomain : check_subdomains)
      if (_problem->getCoordSystem(subdomain) != _coord_system)
        mooseError("The TensorMechanics action requires all subdomains to have the same coordinate system.");
  }
}

void
TensorMechanicsAction::actOutputGeneration()
{
  //
  // Add variables (optional)
  //
  if (_current_task == "add_aux_variable" && getParam<bool>("add_variables"))
  {
    // Loop through output aux variables
    for (auto out : _generate_output)
    {
      // Create output helper aux variables
      _problem->addAuxVariable(out,
                               FEType(Utility::string_to_enum<Order>("CONSTANT"),
                                      Utility::string_to_enum<FEFamily>("MONOMIAL")),
                               _subdomain_id_union.empty() ? nullptr : &_subdomain_id_union);
    }
  }

  //
  // Add output AuxKernels
  //
  else if (_current_task == "add_aux_kernel")
  {
    // Loop through output aux variables
    for (auto out : _generate_output)
    {
      std::string type = "";
      InputParameters params = emptyInputParameters();

      // RankTwoAux
      for (const auto & r2a : _ranktwoaux_table)
        for (unsigned int a = 0; a < 3; ++a)
          for (unsigned int b = 0; b < 3; ++b)
            if (r2a.first + '_' + _component_table[a] + _component_table[b] == out)
            {
              type = "RankTwoAux";
              params = _factory.getValidParams(type);
              params.set<MaterialPropertyName>("rank_two_tensor") = r2a.second;
              params.set<unsigned int>("index_i") = a;
              params.set<unsigned int>("index_j") = b;
            }

      // RankTwoScalarAux
      for (const auto & r2sa : _ranktwoscalaraux_table)
        for (const auto & t : r2sa.second.second)
          if (r2sa.first + '_' + t == out)
          {
            const auto r2a = _ranktwoaux_table.find(t);
            if (r2a != _ranktwoaux_table.end())
            {
              type = "RankTwoScalarAux";
              params = _factory.getValidParams(type);
              params.set<MaterialPropertyName>("rank_two_tensor") = r2a->second;
              params.set<MooseEnum>("scalar_type") = r2sa.second.first;
            }
            else
              mooseError("Internal error. The permitted tensor shortcuts in '_ranktwoscalaraux_table' must be keys in the '_ranktwoaux_table'.");
          }

      if (type != "")
      {
        params.set<AuxVariableName>("variable") = out;
        params.set<MultiMooseEnum>("execute_on") = "timestep_end";
        _problem->addAuxKernel(type, out, params);
      }
      else
        mooseError("Unable to add output AuxKernel");
    }
  }
}

void
TensorMechanicsAction::actGatherActionParameters()
{
  //
  // Gather info about all other master actions when we add variables
  //
  if (_current_task == "validate_coordinate_systems" && getParam<bool>("add_variables"))
  {
    auto actions = _awh.getActions<TensorMechanicsAction>();
    for (const auto & action : actions)
    {
      const auto size_before = _subdomain_id_union.size();
      const auto added_size = action->_subdomain_ids.size();
      _subdomain_id_union.insert(action->_subdomain_ids.begin(), action->_subdomain_ids.end());
      const auto size_after = _subdomain_id_union.size();

      if (size_after != size_before + added_size)
        mooseError("The block restrictions in the TensorMechanics/Master actions must be non-overlapping.");

      if (added_size == 0 && actions.size() > 1)
        mooseError("No TensorMechanics/Master action can be block unrestricted if more than one TensorMechanics/Master action is specified.");
    }
  }
}

std::string
TensorMechanicsAction::getKernelType()
{
  std::map<Moose::CoordinateSystemType, std::string> type_map = {
      {Moose::COORD_XYZ, "StressDivergenceTensors"},
      {Moose::COORD_RZ, "StressDivergenceRZTensors"},
      {Moose::COORD_RSPHERICAL, "StressDivergenceRSphericalTensors"}};

  // choose kernel type based on coordinate system
  auto type_it = type_map.find(_coord_system);
  if (type_it != type_map.end())
    return type_it->second;
  else
    mooseError("Unsupported coordinate system");
}

InputParameters
TensorMechanicsAction::getKernelParameters(std::string type)
{
  InputParameters params = _factory.getValidParams(type);
  params.applyParameters(parameters(), {"displacements", "use_displaced_mesh", "save_in", "diag_save_in"});

  params.set<std::vector<VariableName>>("displacements") = _coupled_displacements;
  params.set<bool>("use_displaced_mesh") = _use_displaced_mesh;

  // deprecated
  if (parameters().isParamValid("temp"))
  {
    params.set<NonlinearVariableName>("temperature") = getParam<NonlinearVariableName>("temp");
    mooseDeprecated("Use 'temperature' instead of 'temp'");
  }

  return params;
}
