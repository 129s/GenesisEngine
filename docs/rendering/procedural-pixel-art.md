# 程序生成像素美术 · 总览与流水线

目标：在“全分辨率 × 像素风 × 零素材”约束下，用基础图元和程序化材质生成可复用的像素风美术，覆盖场景、道具与标识，不依赖外部贴图。

流水线（每个对象/片区的绘制顺序）
1) 几何遮罩（Geometry Mask）：用矩形/多边形/圆角矩形勾勒对象体块，限制后续填充范围。
2) 基础材质填充（Material Fill）：调用材质函数（如 Wood/Brick/Metal），按有限色带+抖动生成纹理。
3) 分片/接缝（Panel/Seams）：木板缝/砖缝/金属带/砂浆等，1px 线条与轻微色阶差。
4) 光照与AO（Lighting/AO）：按统一光向（建议左上→右下）加 1px 高光/暗边、接缝 AO 阴影（线影或网点）。
5) 五金与细件（Fittings）：铆钉/锁舌/把手/铭牌等几何小件（2–3px 单元）。
6) 磨损与贴花（Wear/Decals）：擦边、划痕、污渍、裂纹、印章/告示贴纸等（受 seed 与位置驱动）。
7) 标识与文字（Signage/Text）：字母/单字图标、招牌与标签（像素字库）。

基本约束
- 均使用整数像素坐标/线宽；禁用抗锯齿与连续半透明；渐变以“色阶+有序抖动”实现。
- 图案与噪声的“相位锚定”固定在世界/对象原点，避免相机移动时花纹漂移。
- 颜色来自小型色带（3–5 阶），全局调色板 16/32 色，便于统一风格。

接口约定（伪）
- `MaterialFn(px, py, seed, params) -> shadeIndex|rgba`（基于解析噪声/模式）；
- `ToneRamp{steps[], paletteId}`（色带定义）；
- `Pattern{kind:Hatch45|Cross|Checker|Dots, phase, density}`（图案）；
- `WearMask(px, py, seed, params) -> 0..1`（边缘/手触区域加权）。

与世界数据对齐
- 种子：`seed = hash(worldSeed, sceneId, objectId, layerTag)`；
- 场景主题：Scene/Interactive 的 `metadata.theme/material` 指示材质与参数范围；
- 生成结果不回写世界模型，仅渲染层消费。

## 库拆分建议：生成 vs 渲染

- **素材生成库（Procedural Asset Library）职责**
  - 提供材质、图案、磨损、贴花等生成函数，输出中立的 `PixelLayer/Mask` 或参数化描述（例如 `MaterialDescriptor`、`DecalDescriptor`）。
  - 负责 seed 管理、相位锚定、ToneRamp/Palette 查表以及缓存策略（同一 seed + 参数组合只生成一次，供多个对象复用）。
  - 对外暴露纯函数式接口，便于离线预烘焙或测试；不依赖渲染上下文。
- **渲染库职责**
  - 读取 `WorldAtlas`/`SimulationSnapshot`，结合素材生成库产出的素材描述，完成 draw list 组装与像素输出。
  - 管理图层顺序、批次、相机、Overlay 与策略；对素材只关心“怎么摆放”和“在哪个批次绘制”。
  - 允许通过依赖注入选择具体素材生成实现（默认零素材版本，可扩展为自定义材质包）。
- **接口建议**
  - 定义中间表示（IR）：`struct ProceduralAsset { PixelLayer layers[]; Rect bounds; Metadata tags; }`，渲染库按场景需求把 IR 投射到最终像素。
  - 支持懒加载：渲染库按需请求素材，生成库依据参数返回缓存引用或现算结果。
  - 保持确定性：生成库必须满足 `f(params, seed) -> deterministic output`，渲染库负责在帧内复用相同引用，避免重复生成。

## 世界生成期预烘焙 vs 渲染期即时生成

- **生成期预烘焙（WorldGen Bake）**
  - 在世界生成/载入流程中，根据 Scene/Interactive Schema 批量调用素材生成库，将结果序列化为 `ProceduralAsset` 缓存或直接存入磁盘。
  - 优势：渲染时 0 计算成本，只做解包与摆放；适合完全静态的设施、地形或故事节点。
  - 风险：需要追加缓存格式与版本管理；世界状态发生改动（道具磨损、贴花变化）时需重新触发烘焙。
  - 建议：将烘焙产物存放在 `world_generation` 管线的输出目录，并为运行期提供“版本号 + seed”校验，确保缓存与世界状态一致。
- **渲染期即时生成（On-Demand）**
  - 渲染库在首次请求时调用素材库生成，并将结果缓存到内存/磁盘。
  - 优势：流程简单，对动态变化（磨损随事件更新、玩家DIY）响应灵活；只需存 seed 与参数即可重建。
  - 风险：首帧存在生成突刺，需要异步/后台线程或帧预算控制。
- **折衷方案**
  - 允许素材库同时支持“预烘焙目录 + 运行时懒加载”，渲染库先尝试加载烘焙产物，无则回退即时生成。
  - 对于会在游戏内被修改的对象（破损门、贴纸被撕），优先保留即时生成路径，以 seed 与变更参数为准生成新版本，并更新缓存标签。

## 预烘焙缓存二进制格式（提案）

### 设计目标
- 支持一个文件存储同一 Scene/区域的多个 `ProceduralAsset`，减少 IO。
- 兼容 16/32 色调色板或 RGBA 输出，允许每层指定混合/用途。
- 保证跨平台一致的解码（显式 endianness、定长字段），方便版本迁移。
- 快速定位：通过 TOC（table of contents）按 hash/key 检索，渲染库可 O(1) 命中。
- 可选压缩：首选 LZ4 或 RLE，避免依赖复杂库。

### 文件结构（自上而下）

| Block | 字节序 | 描述 |
| --- | --- | --- |
| `Header` | little-endian | 固定 32 字节，包含魔数、版本、flag、资产数、TOC 偏移等。 |
| `PaletteBlock` (可选) | little-endian | 存储局部调色板（16/32 色），供 palette 格式的层引用。 |
| `TOC` | little-endian | `assetCount` 个 `AssetEntry`，描述每个资产的 key、尺寸、layer 表偏移等。 |
| `DataBlocks` | - | 逐资产存放层数据、元信息，按 TOC 中的偏移/长度定位。 |

#### Header
```cpp
struct AssetPackHeader {
  char magic[4];        // "PPAB"
  uint16_t version;     // 0x0001
  uint16_t flags;       // bit0=hasPalette, bit1=compressed
  uint32_t assetCount;
  uint32_t reserved;    // 对齐
  uint64_t paletteOffset;
  uint64_t tocOffset;
};
```

#### AssetEntry（TOC）
```cpp
struct AssetEntry {
  uint64_t keyHash;       // hash(worldSeed, sceneId, objectId, variant)
  uint64_t schemaHash;    // Schema/参数签名，失配时需重新生成
  uint32_t width;         // bounds 宽度
  uint32_t height;        // bounds 高度
  uint16_t layerCount;
  uint16_t metadataCount;
  uint64_t layerTableOffset;
  uint64_t metadataOffset;
};
```

- `keyHash` 为渲染库查询的主键；同一个对象的多个变体可通过 `variant` 区分（如不同磨损阶段）。
- `schemaHash` 由素材生成库计算（schema JSON → hash），渲染时若 schemaHash 不匹配则回退即时生成。

#### LayerRecord
```cpp
struct LayerRecord {
  uint8_t layerType;     // 0=Base,1=Seam,2=Lighting,3=Fitting,4=Wear,5=Decal,6=Signage...
  uint8_t blendMode;     // 0=Opaque,1=AlphaStep,2=DitherMask,3=Add,4=Multiply...
  uint16_t pixelFormat;  // 0=Index4 (16 色),1=Index8 (256 色),2=RGBA8888
  int16_t originX;       // 与 asset bounds 左上对齐的偏移
  int16_t originY;
  uint16_t reserved;
  uint64_t dataOffset;   // 相对于文件头
  uint32_t dataLength;   // 压缩后字节数
};
```

- 数据块若 `flags.bit1=1`，默认采用 LZ4；否则使用原始像素或简单 RLE。
- palette 格式层通过索引引用 `PaletteBlock`；RGBA 层直接使用 32bit。

#### Metadata
- 采用简单的 KV 表：`uint16_t keyLen + uint16_t valueLen + bytes`。
- 常见键：`"seed"`, `"material"`, `"toneRampId"`, `"wearLevel"`, `"variant"`。
- 渲染库可根据 metadata 选择 overlay/特效。

### 使用流程
1. 世界生成阶段调用素材生成库，得到 `ProceduralAsset` 后写入 `.ppab`（可按 Scene 分组）。
2. 客户端启动时建立索引：读取 Header/TOC，构建 `hash -> AssetEntry` 映射。
3. 渲染库请求素材时先查索引，如命中则按 `LayerTable` 定位数据块并解码；若 `schemaHash` 不匹配或解码失败，回退即时生成并可选择刷新缓存。
4. 对运行期发生改变的对象，可保留即时生成路径并更新内存缓存，必要时写入新的 `.ppab`（需版本管理）。

### 版本与兼容
- `version` 递增，读取方根据版本决定解析策略；建议额外记录 `buildCommit` / `schemaVersion` 于 Header 的保留字段。
- 建议维持小端编码，必要时在 flags 中标记 `endianness`。
- 当 palette 体系大幅变更时，可新建 `PaletteBlock` 版本，旧客户端可拒绝加载并回退即时生成。

参考
- 材质库详见 `materials.md`；图案与抖动见 `patterns-and-dither.md`；道具 Schema 见 `props-schema.md`；灯光与磨损见 `lighting-style.md`、`wear-and-decals.md`；确定性策略见 `seed-determinism.md`。
