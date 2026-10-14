def register_modules(registry):
    registry.register(
        name = "kernel/sched/lenovohypersched/lenovohyperSched",
        out = "lenovohyperSched.ko",
        config = "CONFIG_LENOVO_HYPERSCHED",
        srcs = [
            # do not sort
            "kernel/sched/lenovohypersched/lhsched_main.c",
            "kernel/sched/lenovohypersched/lhsched_rwsem.c",
            "kernel/sched/lenovohypersched/lhsched_mutex.c",
            "kernel/sched/lenovohypersched/lhsched_proc.c",
            "kernel/sched/lenovohypersched/lhsched_prio.c",
            "kernel/sched/lenovohypersched/lhsched_boost.c",
            "kernel/sched/lenovohypersched/lhsched_runtime.c",
            "kernel/sched/lenovohypersched/lhsched.h",
            "kernel/sched/lenovohypersched/schedux.h",
        ],
    )
