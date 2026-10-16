def register_modules(registry):
    registry.register(
        name = "net/sched/sch_mqprio",
        out = "sch_mqprio.ko",
        config = "CONFIG_NET_SCH_MQPRIO",
        srcs = [
            # do not sort
            "net/sched/sch_mqprio.c",
            "net/sched/sch_mqprio_lib.c",
            "net/sched/sch_mqprio_lib.h",
        ],
    )

    registry.register(
        name = "net/sched/sch_cbs",
        out = "sch_cbs.ko",
        config = "CONFIG_NET_SCH_CBS",
        srcs = [
            # do not sort
            "net/sched/sch_cbs.c",
        ],
    )

    registry.register(
        name = "net/sched/cls_flower",
        out = "cls_flower.ko",
        config = "CONFIG_NET_CLS_FLOWER",
        srcs = [
            # do not sort
            "net/sched/cls_flower.c",
        ],
    )
    registry.register(
        name = "net/sched/sch_etf",
        out = "sch_etf.ko",
        config = "CONFIG_NET_SCH_ETF",
        srcs = [
            # do not sort
            "net/sched/sch_etf.c",
        ],
    )
