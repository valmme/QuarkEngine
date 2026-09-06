#include "command_line.h"
#include "version.h"
#include <iostream>

void print_version() {
    std::cout << "Quark Engine " << QUARK_ENGINE_VERSION << "\n";
}

void print_usage(const char* program_name) {
    std::cout <<
        "Usage: " << program_name << " [options] [project_path]\n"
        "\n"
        "General:\n"
        "  project_path              Path to the project to open (positional)\n"
        "  --project <path>          Same as above, explicit form\n"
        "  --headless                Run without a window / editor UI\n"
        "  --test                    Run a minimal startup check and exit (implies --headless)\n"
        "  --new-project             Force-recreate the project even if a valid one exists\n"
        "  -h, --help                Show this help text and exit\n"
        "  --version                 Show the engine version and exit\n"
        "\n"
        "Rendering:\n"
        "  --renderer <opengl|vulkan> Override the renderer backend from preferences\n"
        "  --vsync <on|off>           Override vsync from preferences\n"
        "  --fps <n>                  Override target FPS (0 = unlimited)\n"
        "\n"
        "Plugins:\n"
        "  --no-plugins               Don't load any plugins on startup\n"
        "  --plugins-dir <path>       Directory to load plugins from (default: \"plugins\")\n"
        "\n"
        "Misc:\n"
        "  --lang <code>               Override the editor language\n"
        "  --log-level <level>         Set log verbosity (trace|info|warn|error)\n"
        "  --no-autosave                Disable autosave for this session\n"
        "  --dump-frame [n]             Print full render state for the first n frames, then exit (debug)\n";
}

static bool has_value(int argc, char** argv, int index) {
    return index + 1 < argc;
}

CommandLineOptions parse_command_line(int argc, char** argv) {
    CommandLineOptions options;

    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        const std::string argument = argv[argument_index];

        if (argument == "--headless") {
            options.headless = true;
        } else if (argument == "--test") {
            options.test_mode = true;
        } else if (argument == "--new-project") {
            options.new_project = true;
        } else if (argument == "-h" || argument == "--help") {
            options.help_requested = true;
        } else if (argument == "--version") {
            options.version_requested = true;
        } else if (argument == "--no-plugins") {
            options.no_plugins = true;
        } else if (argument == "--no-autosave") {
            options.no_autosave = true;
        } else if (argument == "--project") {
            if (has_value(argc, argv, argument_index))
                options.project_path = argv[++argument_index];
        } else if (argument == "--renderer") {
            if (has_value(argc, argv, argument_index)) {
                const std::string value = argv[++argument_index];
                if (value == "opengl") options.renderer_override = RendererOverride::OpenGL;
                else if (value == "vulkan") options.renderer_override = RendererOverride::Vulkan;
                else std::cerr << "Unknown renderer '" << value << "', ignoring.\n";
            }
        } else if (argument == "--vsync") {
            if (has_value(argc, argv, argument_index)) {
                const std::string value = argv[++argument_index];
                if (value == "on") options.vsync_override = TriState::On;
                else if (value == "off") options.vsync_override = TriState::Off;
                else std::cerr << "Unknown vsync value '" << value << "', ignoring.\n";
            }
        } else if (argument == "--fps") {
            if (has_value(argc, argv, argument_index)) {
                const std::string value = argv[++argument_index];
                try {
                    options.fps_override = std::stoi(value);
                } catch (...) {
                    std::cerr << "Invalid --fps value '" << value << "', ignoring.\n";
                }
            }
        } else if (argument == "--plugins-dir") {
            if (has_value(argc, argv, argument_index))
                options.plugins_dir = argv[++argument_index];
        } else if (argument == "--lang") {
            if (has_value(argc, argv, argument_index))
                options.lang_override = argv[++argument_index];
        } else if (argument == "--log-level") {
            if (has_value(argc, argv, argument_index))
                options.log_level = argv[++argument_index];
        } else if (argument == "--dump-frame") {
            if (has_value(argc, argv, argument_index)) {
                try {
                    options.dump_frames = std::stoi(argv[argument_index + 1]);
                    if (options.dump_frames > 0) ++argument_index;
                } catch (...) {
                    options.dump_frames = 2;
                }
            } else {
                options.dump_frames = 2;
            }
        } else if (options.project_path.empty()) {
            options.project_path = argument;
        }
    }

    options.headless = options.headless || options.test_mode;

    if (options.help_requested) print_usage(argv[0]);
    if (options.version_requested) print_version();

    return options;
}