def register_modules(registry):
    registry.register(
        name = "drivers/gpu/drm/bridge/lt9611uxc",
        out = "lt9611uxc.ko",
        config = "CONFIG_DRM_LT9611UXC",
        srcs = [
            # do not sort
            "drivers/gpu/drm/bridge/lt9611uxc.c",
        ],
    )

    registry.register(
        name = "drivers/gpu/drm/bridge/aux-bridge",
        out = "aux-bridge.ko",
        config = "CONFIG_DRM_AUX_BRIDGE",
        srcs = [
            # do not sort
            "drivers/gpu/drm/bridge/aux-bridge.c",
        ],
    )
