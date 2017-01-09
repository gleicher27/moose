[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 10
  ny = 10
[]

[Variables]
  [./u]
  [../]
  [./v]
  [../]
[]

[Kernels]
  [./diff]
    type = CoefDiffusion
    variable = u
    coef = 0.1
  [../]
  [./time]
    type = TimeDerivative
    variable = u
  [../]
  [./diff_v]
    type = Diffusion
    variable = v
  [../]
  [./force_u]
    type = CoupledForce
    variable = u
    v = v
  [../]
  [./force_v]
    type = CoupledForce
    variable = v
    v = u
  [../]
  [./td_v]
    type = TimeDerivative
    variable = v
  [../]
  [./reaction]
    type = ExampleGainReactionRate
    variable = u
  [../]
  [./reaction2]
    type = ExampleLossReactionRate
    variable = v
  [../]
[]

[BCs]
  [./left]
    type = DirichletBC
    variable = u
    boundary = left
    value = 0
  [../]
  [./right]
    type = DirichletBC
    variable = u
    boundary = right
    value = 1
  [../]
  [./left_v]
    type = DirichletBC
    variable = v
    boundary = left
    value = 1
  [../]
  [./right_v]
    type = DirichletBC
    variable = v
    boundary = right
    value = 0
  [../]
[]

[Materials]
  [./MaterialGain_v]
     type = ExampleGainMaterial
     variable = v
     block = 0
     inital_gain = 0.0
     gain_factor = 10.0
  [../]
  [./MaterialLoss]
     type = ExampleLossMaterial
     variable = u
     block = 0
     inital_loss = 0.0
     loss_factor = 10.0
  [../]
[]
[Executioner]
  # Preconditioned JFNK (default)
  type = Transient
  num_steps = 500 
  dt = 0.01
  solve_type = PJFNK
  petsc_options_iname = '-pc_type -pc_hypre_type'
  petsc_options_value = 'hypre boomeramg'
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-14
[]

[Outputs]
  exodus = true
[]
