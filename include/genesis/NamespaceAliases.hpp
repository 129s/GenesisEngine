#pragma once

// 目的：为后续逐步迁移到 `Genesis::` 根命名空间提供过渡层。
// 原则：仅提供 namespace alias；不重命名实现命名空间，避免破坏性变更。

namespace genesis {
namespace agents {}
namespace base {}
namespace core {}
namespace diagnostics {}
namespace messaging {}
namespace sandbox {}
namespace simulation {}
namespace telemetry {}
namespace tests {}
namespace world {}
namespace worldgen {}
} // namespace genesis

namespace Genesis {

namespace Agents = ::genesis::agents;
namespace Base = ::genesis::base;
namespace Core = ::genesis::core;
namespace Diagnostics = ::genesis::diagnostics;
namespace Messaging = ::genesis::messaging;
namespace Sandbox = ::genesis::sandbox;
namespace Simulation = ::genesis::simulation;
namespace Telemetry = ::genesis::telemetry;
namespace Testing = ::genesis::tests;
namespace World = ::genesis::world;
namespace Worldgen = ::genesis::worldgen;

} // namespace Genesis

