#pragma once

#include <cstdint>
#include <cstddef>

namespace swg::plugin
{
    /**
     * Version triple describing the semantic version of a plugin or API surface.
     */
    struct Version
    {
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint16_t patch = 0;
    };

    /**
     * Logging levels that plugins can emit through the host dispatch table.
     */
    enum class LogLevel : std::uint8_t
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical
    };

    /**
     * Narrow string view utility that avoids depending on C++20's std::string_view.
     */
    struct StringView
    {
        const char *data = nullptr;
        std::size_t length = 0;
    };

    /**
     * Host-provided services that plugins can call into. Function pointers are preferred so that the ABI remains C-compatible.
     */
    struct HostDispatch
    {
        void (*log)(LogLevel level, StringView message) = nullptr;
        bool (*registerCommand)(StringView name, void (*callback)(void *userData), void *userData) = nullptr;
        void (*enqueueTask)(void (*task)(void *userData), void *userData) = nullptr;
        void *(*acquireService)(StringView serviceName) = nullptr;
        void (*releaseService)(StringView serviceName, void *service) = nullptr;
    };

    /**
     * Context passed to plugins during initialisation.
     */
    struct HostContext
    {
        Version apiVersion{};
        HostDispatch dispatch{};
    };

    /**
     * Metadata describing a plugin so that the host can expose information in diagnostics and UI.
     */
    struct PluginDescriptor
    {
        StringView name{};
        StringView description{};
        Version pluginVersion{};
        Version compatibleApiMin{1, 0, 0};
        Version compatibleApiMax{1, 0, 0};
    };

    /**
     * Collection of lifecycle callbacks that the plugin exposes.
     */
    struct Lifecycle
    {
        bool (*onLoad)(const HostContext &context) = nullptr;
        void (*onUnload)() = nullptr;
        void (*onTick)(double deltaSeconds) = nullptr;
    };

    /**
     * The signature every plugin entry point must implement. Returning false indicates the plugin declined to load.
     */
    using EntryPoint = bool (*)(const HostContext &context, PluginDescriptor &descriptor, Lifecycle &lifecycle);

    constexpr Version makeVersion(std::uint16_t major, std::uint16_t minor, std::uint16_t patch) noexcept
    {
        return Version{major, minor, patch};
    }

    constexpr bool operator==(const Version &lhs, const Version &rhs) noexcept
    {
        return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
    }

    constexpr bool operator!=(const Version &lhs, const Version &rhs) noexcept
    {
        return !(lhs == rhs);
    }

    constexpr bool operator<(const Version &lhs, const Version &rhs) noexcept
    {
        if (lhs.major != rhs.major)
            return lhs.major < rhs.major;
        if (lhs.minor != rhs.minor)
            return lhs.minor < rhs.minor;
        return lhs.patch < rhs.patch;
    }

    constexpr bool operator>(const Version &lhs, const Version &rhs) noexcept
    {
        return rhs < lhs;
    }

    constexpr bool operator<=(const Version &lhs, const Version &rhs) noexcept
    {
        return !(rhs < lhs);
    }

    constexpr bool operator>=(const Version &lhs, const Version &rhs) noexcept
    {
        return !(lhs < rhs);
    }
}

extern "C" {
    /**
     * Plugins must export a function named `SwgRegisterPlugin` with this signature. The implementation should fill in the
     * descriptor and lifecycle structures before returning true.
     */
    bool SwgRegisterPlugin(const swg::plugin::HostContext &context,
                           swg::plugin::PluginDescriptor &descriptor,
                           swg::plugin::Lifecycle &lifecycle);
}

