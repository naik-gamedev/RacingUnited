-- CLEAN08: presentation-only wheel quaternion/transform math.
-- Native physics remains authoritative; these helpers only compose render poses.

local function WheelQuaternionMultiply(a, b)
    return {
        a[1] * b[1] - a[2] * b[2] - a[3] * b[3] - a[4] * b[4],
        a[1] * b[2] + a[2] * b[1] + a[3] * b[4] - a[4] * b[3],
        a[1] * b[3] - a[2] * b[4] + a[3] * b[1] + a[4] * b[2],
        a[1] * b[4] + a[2] * b[3] - a[3] * b[2] + a[4] * b[1]
    }
end

local function WheelQuaternionFromEulerDegrees(x, y, z)
    local halfX = math.rad(x) * 0.5
    local halfY = math.rad(y) * 0.5
    local halfZ = math.rad(z) * 0.5
    local cx, sx = math.cos(halfX), math.sin(halfX)
    local cy, sy = math.cos(halfY), math.sin(halfY)
    local cz, sz = math.cos(halfZ), math.sin(halfZ)
    return {
        cz * cy * cx + sz * sy * sx,
        cz * cy * sx - sz * sy * cx,
        cz * sy * cx + sz * cy * sx,
        sz * cy * cx - cz * sy * sx
    }
end

local function WheelEulerDegreesFromQuaternion(q)
    local w, x, y, z = q[1], q[2], q[3], q[4]
    local rightX = 1.0 - 2.0 * (y * y + z * z)
    local rightY = 2.0 * (x * y + w * z)
    local rightZ = 2.0 * (x * z - w * y)
    local upZ = 2.0 * (y * z + w * x)
    local forwardZ = 1.0 - 2.0 * (x * x + y * y)
    local rotationY = math.asin(math.max(-1.0, math.min(1.0, -rightZ)))
    local rotationX = math.atan(upZ, forwardZ)
    local rotationZ = math.atan(rightY, rightX)
    return math.deg(rotationX), math.deg(rotationY), math.deg(rotationZ)
end

local function WheelQuaternionConjugate(q)
    return { q[1], -q[2], -q[3], -q[4] }
end

local function WheelRotateVectorByQuaternion(q, x, y, z)
    local vectorQ = { 0.0, x, y, z }
    local rotated = WheelQuaternionMultiply(
        WheelQuaternionMultiply(q, vectorQ),
        WheelQuaternionConjugate(q))
    return rotated[2], rotated[3], rotated[4]
end

local function WheelChassisLocalVectorToRenderWorld(localX, localY, localZ)
    -- Entity.SetMeshNodeAnchoredWorldDelta currently accepts a world-space
    -- vector. The GLB subtree itself is attached to the render-interpolated
    -- chassis Entity, so convert our LOCAL suspension delta through that SAME
    -- render pose. The renderer immediately converts it back through the
    -- instance matrix, yielding the exact intended local vector without ever
    -- consulting stale wheel world centers.
    local rx, ry, rz = Entity.GetWorldRotation(chassisEntity)
    local sx, sy, sz = Entity.GetWorldScale(chassisEntity)
    rx, ry, rz = rx or 0.0, ry or 0.0, rz or 0.0
    sx, sy, sz = sx or 1.0, sy or 1.0, sz or 1.0

    local chassisQ = WheelQuaternionFromEulerDegrees(rx, ry, rz)
    return WheelRotateVectorByQuaternion(
        chassisQ,
        localX * sx,
        localY * sy,
        localZ * sz)
end

local function WheelUprightDeltaEulerDegrees(current, baseline)
    local currentQ = WheelQuaternionFromEulerDegrees(
        current.uprightRotationX or 0.0,
        current.uprightRotationY or 0.0,
        current.uprightRotationZ or 0.0)
    local baselineQ = WheelQuaternionFromEulerDegrees(
        baseline.uprightRotationX or 0.0,
        baseline.uprightRotationY or 0.0,
        baseline.uprightRotationZ or 0.0)
    local deltaQ = WheelQuaternionMultiply(
        currentQ,
        WheelQuaternionConjugate(baselineQ))
    return WheelEulerDegreesFromQuaternion(deltaQ)
end

local function WheelWorldUprightEulerDegrees(telemetry)
    -- GetWheelUprightPose exposes local upright Euler angles, but VA02's GLB
    -- node override asks for a WORLD pose. Use the native world basis vectors
    -- directly so chassis yaw + steering + camber + toe are represented once.
    local rightX = telemetry.wheelRightX or 1.0
    local rightY = telemetry.wheelRightY or 0.0
    local rightZ = telemetry.wheelRightZ or 0.0
    local upY = telemetry.wheelUpY or 1.0
    local upZ = telemetry.wheelUpZ or 0.0
    local forwardY = telemetry.wheelForwardY or 0.0
    local forwardZ = telemetry.wheelForwardZ or 1.0

    local rotationY = math.asin(math.max(-1.0, math.min(1.0, -rightZ)))
    local cosineY = math.cos(rotationY)
    local rotationX = 0.0
    local rotationZ = 0.0
    if math.abs(cosineY) > 0.000001 then
        rotationX = math.atan(upZ, forwardZ)
        rotationZ = math.atan(rightY, rightX)
    else
        rotationX = math.atan(-forwardY, upY)
    end
    return math.deg(rotationX), math.deg(rotationY), math.deg(rotationZ)
end


VehicleVisualTransformMath = VehicleVisualTransformMath or {}
VehicleVisualTransformMath.QuaternionMultiply = WheelQuaternionMultiply
VehicleVisualTransformMath.QuaternionFromEulerDegrees = WheelQuaternionFromEulerDegrees
VehicleVisualTransformMath.EulerDegreesFromQuaternion = WheelEulerDegreesFromQuaternion
VehicleVisualTransformMath.QuaternionConjugate = WheelQuaternionConjugate
VehicleVisualTransformMath.RotateVectorByQuaternion = WheelRotateVectorByQuaternion
VehicleVisualTransformMath.ChassisLocalVectorToRenderWorld = WheelChassisLocalVectorToRenderWorld
VehicleVisualTransformMath.UprightDeltaEulerDegrees = WheelUprightDeltaEulerDegrees
VehicleVisualTransformMath.WorldUprightEulerDegrees = WheelWorldUprightEulerDegrees
