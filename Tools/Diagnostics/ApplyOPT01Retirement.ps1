param(
    [Parameter(Mandatory=$true)][string]$Root
)
$ErrorActionPreference = 'Stop'

# OPT01A: ZIP overlays cannot express deletions. Keep the retirement list explicit
# so an extracted milestone deterministically converges an existing checkout to
# the intended architecture before freshness/audit/validation run.
$retired = @(
    'Engine\HeritageEngine\Graphics\GltfBinary.cpp',
    'Engine\HeritageEngine\Physics\SurfaceField.cpp',
    'Engine\HeritageEngine\Vehicles\Aerodynamics\AerodynamicsSystem.cpp',
    'Engine\HeritageEngine\Vehicles\Aerodynamics\AeroSurface.cpp',
    'Engine\HeritageEngine\Vehicles\Aerodynamics\GroundEffect.cpp',
    'Engine\HeritageEngine\Vehicles\Articulation\ArticulatedVehicle.cpp',
    'Engine\HeritageEngine\Vehicles\Articulation\FifthWheel.cpp',
    'Engine\HeritageEngine\Vehicles\Articulation\TrailerCoupling.cpp',
    'Engine\HeritageEngine\Vehicles\Brakes\ABSController.cpp',
    'Engine\HeritageEngine\Vehicles\Brakes\BrakeSystem.cpp',
    'Engine\HeritageEngine\Vehicles\Brakes\BrakeThermal.cpp',
    'Engine\HeritageEngine\Vehicles\Core\VehicleAssembly.cpp',
    'Engine\HeritageEngine\Vehicles\Core\VehicleSimulationScheduler.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\ChainFinalDrive.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\Clutch.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\Differentials\ActiveDifferential.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\Differentials\LimitedSlipDifferential.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\Differentials\LockedDifferential.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\Differentials\OpenDifferential.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\Gearbox.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\PowerUnit.cpp',
    'Engine\HeritageEngine\Vehicles\Drivetrain\TransferCase.cpp',
    'Engine\HeritageEngine\Vehicles\Dynamics\KartChassisFlex.cpp',
    'Engine\HeritageEngine\Vehicles\Dynamics\MotorcycleLeanDynamics.cpp',
    'Engine\HeritageEngine\Vehicles\Steering\AckermannGeometry.cpp',
    'Engine\HeritageEngine\Vehicles\Steering\PowerSteering.cpp',
    'Engine\HeritageEngine\Vehicles\Steering\SteeringSystem.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Axles\SolidAxle\SolidAxleKinematics.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Axles\TorsionBeam\TorsionBeamKinematics.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Common\SuspensionBumpStop.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Common\SuspensionHeaveSpring.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Common\SuspensionSpringDamper.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Geometry\DoubleWishbone\DoubleWishboneKinematics.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Geometry\MultiLink\MultiLinkKinematics.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Geometry\SemiTrailingArm\SemiTrailingArmKinematics.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Motorcycle\RearLinkage.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Motorcycle\RearSwingarm.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Motorcycle\Telelever.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Motorcycle\TelescopicFork.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Springs\AirSpring.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Springs\CoilSpring.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Springs\HydropneumaticSpring.cpp',
    'Engine\HeritageEngine\Vehicles\Suspension\Springs\LeafSpring.cpp',
    'Engine\HeritageEngine\Vehicles\Tires\LowPressureTireModel.cpp',
    'Engine\HeritageEngine\Vehicles\Tires\TireCarcass3D.cpp',
    'Engine\HeritageEngine\Vehicles\Tires\TireCarcass3D.hpp',
    'Engine\HeritageEngine\Vehicles\Topology\Common\VehicleTopologyCoordinator.cpp',
    'Engine\HeritageEngine\Vehicles\Topology\FourPlusWheel\FourPlusWheelVehicleDynamics.cpp',
    'Engine\HeritageEngine\Vehicles\Topology\ThreeWheel\ThreeWheelVehicleDynamics.cpp',
    'Engine\HeritageEngine\Vehicles\Topology\TwoWheel\TwoWheelVehicleDynamics.cpp',
    'Engine\HeritageEngine\Vehicles\VehicleConfiguration.cpp',
    'Engine\HeritageEngine\Vehicles\Wheels\Fitment\InstalledWheelMassProperties.cpp',
    'Engine\HeritageEngine\Vehicles\Wheels\Fitment\WheelClearance.cpp',
    'Engine\HeritageEngine\Vehicles\Wheels\WheelDynamics.cpp',
    'Engine\HeritageEngine\Vehicles\Wheels\WheelHub.cpp',
    'Modules\RacingUnited\Scripts\UI\Prototype\CloudLabPanel.lua',
    'Modules\RacingUnited\Scripts\Vehicles\Topology\Common.lua',
    'Modules\RacingUnited\Scripts\Vehicles\Topology\FourPlusWheel.lua',
    'Modules\RacingUnited\Scripts\Vehicles\Topology\ThreeWheel.lua',
    'Modules\RacingUnited\Scripts\Vehicles\Topology\TwoWheel.lua'
)

$removed = 0
foreach ($relative in $retired) {
    $path = Join-Path $Root $relative
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
        $removed++
    }
}

# Prune empty directories left behind by scaffold-only branches, deepest first.
$vehicleRoot = Join-Path $Root 'Engine\HeritageEngine\Vehicles'
if (Test-Path -LiteralPath $vehicleRoot) {
    Get-ChildItem -LiteralPath $vehicleRoot -Directory -Recurse |
        Sort-Object { $_.FullName.Length } -Descending |
        ForEach-Object {
            if (-not (Get-ChildItem -LiteralPath $_.FullName -Force | Select-Object -First 1)) {
                Remove-Item -LiteralPath $_.FullName -Force
            }
        }
}
$luaTopology = Join-Path $Root 'Modules\RacingUnited\Scripts\Vehicles\Topology'
if ((Test-Path -LiteralPath $luaTopology) -and -not (Get-ChildItem -LiteralPath $luaTopology -Force | Select-Object -First 1)) {
    Remove-Item -LiteralPath $luaTopology -Force
}

Write-Host "OPT01 retirement convergence: removed $removed stale overlay file(s); repository now matches the milestone deletion set."
exit 0
