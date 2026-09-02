#pragma once
#include <string_view>

namespace heritage::vehicles::suspension
{

enum class SuspensionProviderKind
{
    // Existing authorities retained from SUSP01-SUSP14.
    MacPhersonStrut,
    DoubleWishbone,
    PushrodRockerWishbone,
    RigidLiveAxle,
    LeafSpringLiveAxle,
    MotorcycleForkSwingarm,
    SemiTrailingArm,
    TwistBeam,
    MultiLink,

    // SUSP15+ genuinely distinct geometry.
    SwingAxle,
    SlidingPillar,
    MotorcycleLinkFront,

    // Aliases that deliberately reuse an existing physical authority.
    ChapmanStrutAlias,
    PureTrailingArmAlias,
    DeDionAlias
};

inline constexpr std::string_view suspensionProviderKey(SuspensionProviderKind k)
{
    switch(k)
    {
    case SuspensionProviderKind::MacPhersonStrut: return "macpherson_strut_v1";
    case SuspensionProviderKind::DoubleWishbone: return "double_wishbone_v1";
    case SuspensionProviderKind::PushrodRockerWishbone: return "pushrod_rocker_v1";
    case SuspensionProviderKind::RigidLiveAxle: return "rigid_axle_linkage_v1";
    case SuspensionProviderKind::LeafSpringLiveAxle: return "leaf_live_axle_v1";
    case SuspensionProviderKind::MotorcycleForkSwingarm: return "motorcycle_fork_swingarm_v1";
    case SuspensionProviderKind::SemiTrailingArm: return "semi_trailing_arm_v1";
    case SuspensionProviderKind::TwistBeam: return "twist_beam_v1";
    case SuspensionProviderKind::MultiLink: return "multi_link_v1";
    case SuspensionProviderKind::SwingAxle: return "swing_axle_v1";
    case SuspensionProviderKind::SlidingPillar: return "sliding_pillar_v1";
    case SuspensionProviderKind::MotorcycleLinkFront: return "motorcycle_link_front_v1";
    case SuspensionProviderKind::ChapmanStrutAlias: return "chapman_strut_alias_v1";
    case SuspensionProviderKind::PureTrailingArmAlias: return "trailing_arm_alias_v1";
    case SuspensionProviderKind::DeDionAlias: return "de_dion_alias_v1";
    }
    return "unknown";
}

// Alias targets are integration contracts, not second solvers.
inline constexpr SuspensionProviderKind canonicalProvider(SuspensionProviderKind k)
{
    switch(k)
    {
    case SuspensionProviderKind::ChapmanStrutAlias: return SuspensionProviderKind::MacPhersonStrut;
    case SuspensionProviderKind::PureTrailingArmAlias: return SuspensionProviderKind::SemiTrailingArm;
    case SuspensionProviderKind::DeDionAlias: return SuspensionProviderKind::RigidLiveAxle;
    default: return k;
    }
}

} // namespace heritage::vehicles::suspension
