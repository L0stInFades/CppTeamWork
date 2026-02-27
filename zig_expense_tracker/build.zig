const std = @import("std");

pub fn build(b: *std.Build) void {
    // Default to native CPU for maximum performance on the host machine
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_model = .native,
        },
    });
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "expense_tracker",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            // Strip debug info in release builds to reduce binary size and startup time
            .strip = if (optimize != .Debug) true else null,
        }),
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "运行开销追踪器");
    run_step.dependOn(&run_cmd.step);
}
