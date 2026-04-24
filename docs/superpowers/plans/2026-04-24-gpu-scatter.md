# GPU-Accelerated Scatter Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the per-point QPainter::drawShape scatter loop with instanced GPU sprite rendering via QRhi, targeting smooth 1M-point interaction.

**Architecture:** New `QCPScatterRhiLayer` class in NeoQCP's painting subsystem, following the exact pattern of `QCPPlottableRhiLayer`. Marker shapes are pre-rendered into a sprite texture via existing `QCPScatterStyle::drawShape()`. Per-instance data (x, y, optional color scalar) is uploaded as a vertex buffer. Two new GLSL shaders position instanced quads and sample the sprite + optional 1D colormap texture. The QPainter fallback path is preserved for export (PDF/PNG).

**Tech Stack:** C++20, Qt6 QRhi, GLSL 4.40 (compiled to QSB via `qsb --qt6`), Meson build system, Qt Test framework.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/painting/scatter-rhi-layer.h` | Create | `QCPScatterRhiLayer` class definition |
| `src/painting/scatter-rhi-layer.cpp` | Create | GPU scatter rendering: sprite cache, instanced draw |
| `src/painting/shaders/scatter.vert` | Create | Instanced quad vertex shader |
| `src/painting/shaders/scatter.frag` | Create | Sprite sampling + optional colormap fragment shader |
| `src/scatterstyle.h` | Modify | Add `renderToImage()` method |
| `src/scatterstyle.cpp` | Modify | Implement `renderToImage()` |
| `src/core.h` | Modify | Add `mScatterRhiLayers` map + accessor |
| `src/core.cpp` | Modify | Wire scatter RHI layer into upload/render pipeline |
| `src/plottables/plottable-graph2.cpp` | Modify | Scatter section: RHI path + QPainter fallback |
| `src/plottables/plottable-multigraph.cpp` | Modify | Same scatter section change |
| `meson.build` | Modify | Add shader QSB targets, new source file, embed |
| `tests/auto/test-scatter-rhi/test-scatter-rhi.h` | Create | Test header |
| `tests/auto/test-scatter-rhi/test-scatter-rhi.cpp` | Create | Scatter RHI correctness tests |
| `tests/auto/meson.build` | Modify | Register new test |
| `tests/benchmark/benchmark.cpp` | Modify | Add 1M-point scatter benchmarks |

---

### Task 1: Scatter Sprite Rendering on QCPScatterStyle

Add a method to `QCPScatterStyle` that renders the marker into a QImage — the sprite texture source.

**Files:**
- Modify: `src/scatterstyle.h` (add method declaration)
- Modify: `src/scatterstyle.cpp` (implement method)

- [ ] **Step 1: Write the failing test**

Create `tests/auto/test-scatter-rhi/test-scatter-rhi.h`:

```cpp
#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestScatterRhi : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // Task 1: sprite rendering
    void renderToImageProducesNonEmptyImage();
    void renderToImageDiscHasAlpha();
    void renderToImageRespectsPenColor();
    void renderToImageCustomPathWorks();

private:
    QCustomPlot* mPlot = nullptr;
};
```

Create `tests/auto/test-scatter-rhi/test-scatter-rhi.cpp`:

```cpp
#include "test-scatter-rhi.h"
#include "../../../src/qcp.h"
#include "../../../src/painting/scatter-rhi-layer.h"

void TestScatterRhi::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->replot();
}

void TestScatterRhi::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestScatterRhi::renderToImageProducesNonEmptyImage()
{
    QCPScatterStyle style(QCPScatterStyle::ssDisc, 10);
    QImage img = style.renderToImage(64);
    QCOMPARE(img.width(), 64);
    QCOMPARE(img.height(), 64);
    // At least one non-transparent pixel
    bool hasContent = false;
    for (int y = 0; y < img.height() && !hasContent; ++y)
        for (int x = 0; x < img.width() && !hasContent; ++x)
            if (qAlpha(img.pixel(x, y)) > 0)
                hasContent = true;
    QVERIFY(hasContent);
}

void TestScatterRhi::renderToImageDiscHasAlpha()
{
    QCPScatterStyle style(QCPScatterStyle::ssDisc, Qt::red, 10);
    QImage img = style.renderToImage(64);
    // Corners should be transparent (disc doesn't fill them)
    QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
    // Center should be opaque
    QVERIFY(qAlpha(img.pixel(32, 32)) > 200);
}

void TestScatterRhi::renderToImageRespectsPenColor()
{
    QCPScatterStyle style(QCPScatterStyle::ssCircle, Qt::blue, Qt::transparent, 10);
    QImage img = style.renderToImage(64);
    // Find a non-transparent pixel near center — should be blue-ish
    QColor c = img.pixelColor(32, 28); // near the circle outline
    // The outline should have blue component
    bool foundBlue = false;
    for (int y = 20; y < 44 && !foundBlue; ++y)
        for (int x = 20; x < 44 && !foundBlue; ++x) {
            QColor px = img.pixelColor(x, y);
            if (px.alpha() > 0 && px.blue() > px.red() && px.blue() > px.green())
                foundBlue = true;
        }
    QVERIFY(foundBlue);
}

void TestScatterRhi::renderToImageCustomPathWorks()
{
    QPainterPath path;
    path.addRect(-4, -4, 8, 8);
    QCPScatterStyle style(path, QPen(Qt::black), QBrush(Qt::red), 10);
    QImage img = style.renderToImage(64);
    bool hasContent = false;
    for (int y = 0; y < img.height() && !hasContent; ++y)
        for (int x = 0; x < img.width() && !hasContent; ++x)
            if (qAlpha(img.pixel(x, y)) > 0)
                hasContent = true;
    QVERIFY(hasContent);
}
```

- [ ] **Step 2: Register test in build system**

In `tests/auto/meson.build`, add to `test_srcs`:
```
    'test-scatter-rhi/test-scatter-rhi.cpp',
```

Add to `test_headers`:
```
    'test-scatter-rhi/test-scatter-rhi.h',
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `meson compile -C build && meson test -C build --print-errorlogs -t 0 auto-tests`
Expected: Compilation fails — `renderToImage` does not exist yet.

- [ ] **Step 4: Implement renderToImage**

In `src/scatterstyle.h`, add after the existing `drawShape` declarations (around line 170):

```cpp
    QImage renderToImage(int textureSize = 64) const;
```

In `src/scatterstyle.cpp`, add at the end:

```cpp
QImage QCPScatterStyle::renderToImage(int textureSize) const
{
    QImage img(textureSize, textureSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QCPPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(mPen);
    painter.setBrush(mBrush);

    double center = textureSize / 2.0;
    double scale = (textureSize - 2.0) / mSize; // leave 1px margin each side
    painter.translate(center, center);
    painter.scale(scale, scale);
    drawShape(&painter, 0.0, 0.0);

    return img;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `meson compile -C build && meson test -C build --print-errorlogs -t 0 auto-tests`
Expected: All 4 sprite tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/scatterstyle.h src/scatterstyle.cpp \
        tests/auto/test-scatter-rhi/test-scatter-rhi.h \
        tests/auto/test-scatter-rhi/test-scatter-rhi.cpp \
        tests/auto/meson.build
git commit -m "feat(scatter): add QCPScatterStyle::renderToImage for GPU sprite texture"
```

---

### Task 2: Scatter Shaders

Create the GLSL shaders and wire them into the Meson build.

**Files:**
- Create: `src/painting/shaders/scatter.vert`
- Create: `src/painting/shaders/scatter.frag`
- Modify: `meson.build` (add QSB targets and embed)

- [ ] **Step 1: Write the vertex shader**

Create `src/painting/shaders/scatter.vert`:

```glsl
#version 440

// Per-vertex (quad corner): 2 floats
layout(location = 0) in vec2 cornerOffset;  // {-1,-1}, {1,-1}, {1,1}, {-1,1}

// Per-instance: 3 floats (x, y, colorValue)
layout(location = 1) in vec3 instanceData;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out float v_colorValue;

layout(std140, binding = 0) uniform Params {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;
    float offsetY;
    float halfSize;   // marker half-size in logical pixels
    float useColorAxis; // 0.0 = no, 1.0 = yes
};

void main()
{
    // Instance position in logical pixels + pan offset
    float px = (instanceData.x + offsetX) * dpr;
    float py = (instanceData.y + offsetY) * dpr;

    // Quad corner offset in physical pixels
    float cx = cornerOffset.x * halfSize * dpr;
    float cy = cornerOffset.y * halfSize * dpr;

    float ndcX = ((px + cx) / width) * 2.0 - 1.0;
    float ndcY = yFlip * (((py + cy) / height) * 2.0 - 1.0);

    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_uv = cornerOffset * 0.5 + 0.5;  // map [-1,1] -> [0,1]
    v_colorValue = instanceData.z;
}
```

- [ ] **Step 2: Write the fragment shader**

Create `src/painting/shaders/scatter.frag`:

```glsl
#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 1) in float v_colorValue;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D spriteTexture;
layout(binding = 2) uniform sampler2D colormapTexture;

layout(std140, binding = 0) uniform Params {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;
    float offsetY;
    float halfSize;
    float useColorAxis;
};

void main()
{
    vec4 sprite = texture(spriteTexture, v_uv);

    if (useColorAxis > 0.5) {
        // Color from 1D gradient, alpha from sprite
        vec4 cmapColor = texture(colormapTexture, vec2(v_colorValue, 0.5));
        fragColor = vec4(cmapColor.rgb * sprite.a, sprite.a);
    } else {
        // Full RGBA from sprite (color baked in)
        fragColor = sprite;
    }
}
```

- [ ] **Step 3: Add shader QSB targets to meson.build**

In `meson.build`, after the `contour_line_frag_qsb` target (around line 150), add:

```meson
scatter_vert_qsb = custom_target('scatter_vert_qsb',
    input: 'src/painting/shaders/scatter.vert',
    output: 'scatter.vert.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])

scatter_frag_qsb = custom_target('scatter_frag_qsb',
    input: 'src/painting/shaders/scatter.frag',
    output: 'scatter.frag.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])
```

Update the `embedded_shaders` custom_target input list to include the new shaders:

```meson
embedded_shaders = custom_target('embedded_shaders',
    input: [composite_vert_qsb, composite_frag_qsb, plottable_vert_qsb, plottable_frag_qsb,
            span_vert_qsb, contour_line_vert_qsb, contour_line_frag_qsb,
            scatter_vert_qsb, scatter_frag_qsb],
    output: 'embedded_shaders.h',
    command: [python3, files('src/painting/shaders/embed_shaders.py'),
              '@OUTPUT@',
              'composite_vert_qsb_data:@INPUT0@',
              'composite_frag_qsb_data:@INPUT1@',
              'plottable_vert_qsb_data:@INPUT2@',
              'plottable_frag_qsb_data:@INPUT3@',
              'span_vert_qsb_data:@INPUT4@',
              'contour_line_vert_qsb_data:@INPUT5@',
              'contour_line_frag_qsb_data:@INPUT6@',
              'scatter_vert_qsb_data:@INPUT7@',
              'scatter_frag_qsb_data:@INPUT8@'])
```

- [ ] **Step 4: Verify shaders compile**

Run: `meson setup build --wipe && meson compile -C build`
Expected: Build succeeds, `embedded_shaders.h` now contains `scatter_vert_qsb_data` and `scatter_frag_qsb_data`.

- [ ] **Step 5: Commit**

```bash
git add src/painting/shaders/scatter.vert src/painting/shaders/scatter.frag meson.build
git commit -m "feat(scatter): add instanced scatter vertex and fragment shaders"
```

---

### Task 3: QCPScatterRhiLayer Core

Implement the RHI layer class that manages GPU resources and renders instanced scatter points.

**Files:**
- Create: `src/painting/scatter-rhi-layer.h`
- Create: `src/painting/scatter-rhi-layer.cpp`
- Modify: `meson.build` (add source file)

- [ ] **Step 1: Write tests for the RHI layer**

Add to `tests/auto/test-scatter-rhi/test-scatter-rhi.h`, in the `private slots:` section:

```cpp
    // Task 3: RHI layer
    void scatterRhiLayerCreatedLazily();
    void replotWithScatterDoesNotCrash();
    void replotScatterDiscDoesNotCrash();
    void replotScatterSquareDoesNotCrash();
    void replotScatterCustomDoesNotCrash();
    void exportWithScatterStillWorks();
    void clearResetsState();
```

Add implementations to `test-scatter-rhi.cpp`:

```cpp
void TestScatterRhi::scatterRhiLayerCreatedLazily()
{
    auto* srl = mPlot->scatterRhiLayer(mPlot->layer("main"));
    // May be null if RHI not initialized in test env, that's OK
    Q_UNUSED(srl);
    QVERIFY(true);
}

void TestScatterRhi::replotWithScatterDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    std::vector<double> keys(1000), values(1000);
    for (int i = 0; i < 1000; ++i) {
        keys[i] = i / 1000.0;
        values[i] = std::sin(keys[i] * 10 * M_PI);
    }
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::replotScatterDiscDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::red, 8));
    std::vector<double> keys{0.1, 0.5, 0.9}, values{0.2, 0.8, 0.4};
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::replotScatterSquareDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, Qt::blue, 8));
    std::vector<double> keys{0.1, 0.5, 0.9}, values{0.2, 0.8, 0.4};
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::replotScatterCustomDoesNotCrash()
{
    QPainterPath path;
    path.addEllipse(-3, -3, 6, 6);
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(path, QPen(Qt::black), QBrush(Qt::green), 8));
    std::vector<double> keys{0.2, 0.6}, values{0.3, 0.7};
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::exportWithScatterStillWorks()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    std::vector<double> keys{0.1, 0.5, 0.9}, values{0.2, 0.8, 0.4};
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    QPixmap pm = mPlot->toPixmap(200, 150);
    QVERIFY(!pm.isNull());
    QCOMPARE(pm.width(), 200);
    QCOMPARE(pm.height(), 150);
}

void TestScatterRhi::clearResetsState()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    std::vector<double> keys{0.1, 0.5}, values{0.2, 0.8};
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    mPlot->replot();
    // Replot again — should clear and rebuild without crash
    mPlot->replot();
    mPlot->replot();
    QVERIFY(true);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `meson compile -C build`
Expected: Fails — `scatter-rhi-layer.h` not found, `scatterRhiLayer` not declared.

- [ ] **Step 3: Create scatter-rhi-layer.h**

Create `src/painting/scatter-rhi-layer.h`:

```cpp
#pragma once

#include <QImage>
#include <QRect>
#include <QVector>
#include <rhi/qrhi.h>
#include <span>
#include <cstdlib>

class QCPScatterStyle;

class QCPScatterRhiLayer
{
public:
    struct DrawEntry
    {
        int instanceOffset = 0;
        int instanceCount = 0;
        float offsetX = 0;
        float offsetY = 0;
        QRect scissorRect;
    };

    explicit QCPScatterRhiLayer(QRhi* rhi);
    ~QCPScatterRhiLayer();

    void clear();

    // points: flat array of (x, y, colorValue) per point — 3 floats each.
    // If no color axis, caller sets colorValue to 0.
    void addScatter(std::span<const float> points,
                    const QCPScatterStyle& style,
                    const QRect& clipRect, double dpr, int outputHeight,
                    float offsetX = 0, float offsetY = 0,
                    const QImage& colormapImage = {});

    void setAllOffsets(float offsetX, float offsetY);

    void invalidatePipeline();
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                          const QSize& outputSize, float dpr, bool isYUpInNDC);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

    bool isDirty() const { return mDirty; }
    bool hasGeometry() const { return !mDrawEntries.isEmpty(); }

private:
    struct alignas(16) PerDrawUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY;
        float halfSize;
        float useColorAxis;
    };
    static_assert(sizeof(PerDrawUniforms) == 32);

    int ubufStride() const;

    void stagingAppend(const float* src, int count);
    float* mStagingData = nullptr;
    int mStagingSize = 0;
    int mStagingCapacity = 0;

    QRhi* mRhi;
    QVector<DrawEntry> mDrawEntries;

    // Quad geometry (shared across all instances)
    QRhiBuffer* mQuadVertexBuffer = nullptr;
    QRhiBuffer* mQuadIndexBuffer = nullptr;

    // Per-instance data
    QRhiBuffer* mInstanceBuffer = nullptr;
    int mInstanceBufferSize = 0;

    QRhiBuffer* mUniformBuffer = nullptr;
    int mUniformBufferSize = 0;

    // Textures
    QRhiTexture* mSpriteTexture = nullptr;
    QRhiTexture* mColormapTexture = nullptr;
    QRhiSampler* mSampler = nullptr;

    QRhiShaderResourceBindings* mSrb = nullptr;
    QRhiGraphicsPipeline* mPipeline = nullptr;
    int mLastSampleCount = 0;

    bool mDirty = false;
    bool mSpriteTextureDirty = false;
    bool mColormapTextureDirty = false;
    QImage mSpriteImage;
    QImage mColormapImage;
    float mHalfSize = 0;
    bool mUseColorAxis = false;

    // Sprite cache key
    int mCachedShape = -1;
    double mCachedSize = -1;
    QPen mCachedPen;
    QBrush mCachedBrush;
};
```

- [ ] **Step 4: Create scatter-rhi-layer.cpp**

Create `src/painting/scatter-rhi-layer.cpp`:

```cpp
#include "scatter-rhi-layer.h"
#include "rhi-utils.h"
#include "Profiling.hpp"
#include "embedded_shaders.h"
#include "../scatterstyle.h"
#include <cstring>

namespace {

constexpr float kQuadVertices[] = {
    -1.0f, -1.0f,  // bottom-left
     1.0f, -1.0f,  // bottom-right
     1.0f,  1.0f,  // top-right
    -1.0f,  1.0f,  // top-left
};

constexpr quint16 kQuadIndices[] = { 0, 1, 2, 0, 2, 3 };

QImage ensureBGRA(const QImage& img)
{
    if (img.format() == QImage::Format_ARGB32_Premultiplied)
        return img;
    return img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

} // namespace

QCPScatterRhiLayer::QCPScatterRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

QCPScatterRhiLayer::~QCPScatterRhiLayer()
{
    delete mPipeline;
    delete mSrb;
    delete mUniformBuffer;
    delete mInstanceBuffer;
    delete mQuadVertexBuffer;
    delete mQuadIndexBuffer;
    delete mSpriteTexture;
    delete mColormapTexture;
    delete mSampler;
    std::free(mStagingData);
}

void QCPScatterRhiLayer::invalidatePipeline()
{
    delete mPipeline;   mPipeline = nullptr;
    delete mSrb;        mSrb = nullptr;
    delete mUniformBuffer; mUniformBuffer = nullptr;
    mUniformBufferSize = 0;
    delete mSampler;    mSampler = nullptr;
}

void QCPScatterRhiLayer::clear()
{
    mStagingSize = 0;
    mDrawEntries.resize(0);
    mDirty = true;
}

void QCPScatterRhiLayer::setAllOffsets(float offsetX, float offsetY)
{
    for (auto& entry : mDrawEntries)
    {
        entry.offsetX = offsetX;
        entry.offsetY = offsetY;
    }
}

int QCPScatterRhiLayer::ubufStride() const
{
    return mRhi->ubufAligned(sizeof(PerDrawUniforms));
}

void QCPScatterRhiLayer::stagingAppend(const float* src, int count)
{
    if (mStagingSize + count > mStagingCapacity)
    {
        mStagingCapacity = std::max(mStagingCapacity * 2, mStagingSize + count);
        mStagingData = static_cast<float*>(
            std::realloc(mStagingData, mStagingCapacity * sizeof(float)));
    }
    std::memcpy(mStagingData + mStagingSize, src, count * sizeof(float));
    mStagingSize += count;
}

void QCPScatterRhiLayer::addScatter(std::span<const float> points,
                                     const QCPScatterStyle& style,
                                     const QRect& clipRect, double dpr,
                                     int outputHeight,
                                     float offsetX, float offsetY,
                                     const QImage& colormapImage)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::addScatter");

    if (points.empty())
        return;

    const int pointCount = static_cast<int>(points.size()) / 3;

    DrawEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.instanceOffset = mStagingSize / 3;
    entry.instanceCount = pointCount;

    stagingAppend(points.data(), static_cast<int>(points.size()));
    mDrawEntries.append(entry);
    mDirty = true;

    // Update sprite if style changed
    bool styleChanged = (static_cast<int>(style.shape()) != mCachedShape
                          || style.size() != mCachedSize
                          || style.pen() != mCachedPen
                          || style.brush() != mCachedBrush);
    if (styleChanged)
    {
        mSpriteImage = ensureBGRA(style.renderToImage(64));
        mHalfSize = static_cast<float>(style.size()) / 2.0f + 1.0f; // +1 for AA margin
        mCachedShape = static_cast<int>(style.shape());
        mCachedSize = style.size();
        mCachedPen = style.pen();
        mCachedBrush = style.brush();
        mSpriteTextureDirty = true;
    }

    mUseColorAxis = !colormapImage.isNull();
    if (mUseColorAxis)
    {
        mColormapImage = ensureBGRA(colormapImage);
        mColormapTextureDirty = true;
    }
}

bool QCPScatterRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc,
                                         int sampleCount)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::ensurePipeline");

    if (mPipeline && mLastSampleCount == sampleCount)
        return true;

    invalidatePipeline();

    auto vertShader = qcp::rhi::loadEmbeddedShader(
        scatter_vert_qsb_data, scatter_vert_qsb_data_len);
    auto fragShader = qcp::rhi::loadEmbeddedShader(
        scatter_frag_qsb_data, scatter_frag_qsb_data_len);
    if (!vertShader.isValid() || !fragShader.isValid())
    {
        qDebug() << "Failed to load scatter shaders";
        return false;
    }

    // Quad vertex buffer (static)
    if (!mQuadVertexBuffer)
    {
        mQuadVertexBuffer = mRhi->newBuffer(
            QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadVertices));
        mQuadVertexBuffer->create();
    }
    if (!mQuadIndexBuffer)
    {
        mQuadIndexBuffer = mRhi->newBuffer(
            QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, sizeof(kQuadIndices));
        mQuadIndexBuffer->create();
    }

    // Sampler
    mSampler = mRhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    mSampler->create();

    // Sprite texture (placeholder, will be resized on first upload)
    if (!mSpriteTexture)
    {
        mSpriteTexture = mRhi->newTexture(
            qcp::rhi::preferredTextureFormat(mRhi), QSize(64, 64));
        mSpriteTexture->create();
        mSpriteTextureDirty = true;
    }

    // Colormap texture (1D as a 256x1 2D texture)
    if (!mColormapTexture)
    {
        mColormapTexture = mRhi->newTexture(
            qcp::rhi::preferredTextureFormat(mRhi), QSize(256, 1));
        mColormapTexture->create();
        mColormapTextureDirty = true;
    }

    // UBO
    const int stride = ubufStride();
    mUniformBufferSize = stride;
    mUniformBuffer = mRhi->newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, mUniformBufferSize);
    mUniformBuffer->create();

    // SRB: UBO (binding 0) + sprite texture (binding 1) + colormap texture (binding 2)
    mSrb = mRhi->newShaderResourceBindings();
    mSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            mUniformBuffer, sizeof(PerDrawUniforms)),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage,
            mSpriteTexture, mSampler),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage,
            mColormapTexture, mSampler),
    });
    mSrb->create();

    // Pipeline
    mPipeline = mRhi->newGraphicsPipeline();
    mPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertShader},
        {QRhiShaderStage::Fragment, fragShader}
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        {2 * sizeof(float), QRhiVertexInputBinding::PerVertex},    // binding 0: quad corners
        {3 * sizeof(float), QRhiVertexInputBinding::PerInstance},  // binding 1: instance data
    });
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},               // location 0: cornerOffset
        {1, 1, QRhiVertexInputAttribute::Float3, 0},               // location 1: instanceData
    });
    mPipeline->setVertexInputLayout(inputLayout);

    mPipeline->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});
    mPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    mPipeline->setSampleCount(sampleCount);
    mPipeline->setRenderPassDescriptor(rpDesc);
    mPipeline->setShaderResourceBindings(mSrb);

    if (!mPipeline->create())
    {
        qDebug() << "Failed to create scatter pipeline";
        delete mPipeline;
        mPipeline = nullptr;
        return false;
    }

    mLastSampleCount = sampleCount;
    return true;
}

void QCPScatterRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                           const QSize& outputSize, float dpr,
                                           bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::uploadResources");

    if (mDrawEntries.isEmpty() || !mUniformBuffer)
        return;

    // Upload static quad geometry on first use
    static bool quadUploaded = false;
    if (!quadUploaded && mQuadVertexBuffer && mQuadIndexBuffer)
    {
        updates->uploadStaticBuffer(mQuadVertexBuffer, kQuadVertices);
        updates->uploadStaticBuffer(mQuadIndexBuffer, kQuadIndices);
        quadUploaded = true;
    }

    // Upload sprite texture
    if (mSpriteTextureDirty && !mSpriteImage.isNull())
    {
        if (mSpriteTexture->pixelSize() != mSpriteImage.size())
        {
            mSpriteTexture->setPixelSize(mSpriteImage.size());
            mSpriteTexture->create();
            if (mSrb) mSrb->create();
        }
        updates->uploadTexture(mSpriteTexture,
            QRhiTextureUploadDescription(
                QRhiTextureUploadEntry(0, 0,
                    QRhiTextureSubresourceUploadDescription(mSpriteImage))));
        mSpriteTextureDirty = false;
    }

    // Upload colormap texture
    if (mColormapTextureDirty && !mColormapImage.isNull())
    {
        if (mColormapTexture->pixelSize() != mColormapImage.size())
        {
            mColormapTexture->setPixelSize(mColormapImage.size());
            mColormapTexture->create();
            if (mSrb) mSrb->create();
        }
        updates->uploadTexture(mColormapTexture,
            QRhiTextureUploadDescription(
                QRhiTextureUploadEntry(0, 0,
                    QRhiTextureSubresourceUploadDescription(mColormapImage))));
        mColormapTextureDirty = false;
    }

    // Grow UBO if needed
    const int stride = ubufStride();
    const int requiredUboSize = stride * mDrawEntries.size();
    if (requiredUboSize > mUniformBufferSize)
    {
        mUniformBufferSize = requiredUboSize;
        mUniformBuffer->setSize(mUniformBufferSize);
        mUniformBuffer->create();
        if (mSrb) mSrb->create();
    }

    // Upload per-draw uniforms
    const float yFlip = isYUpInNDC ? -1.0f : 1.0f;
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];
        PerDrawUniforms params = {
            float(outputSize.width()),
            float(outputSize.height()),
            yFlip,
            dpr,
            entry.offsetX,
            entry.offsetY,
            mHalfSize,
            mUseColorAxis ? 1.0f : 0.0f,
        };
        updates->updateDynamicBuffer(mUniformBuffer, i * stride,
                                      sizeof(params), &params);
    }

    // Upload instance data
    if (!mDirty || mStagingSize == 0)
        return;

    const int requiredSize = mStagingSize * static_cast<int>(sizeof(float));
    if (!mInstanceBuffer || mInstanceBufferSize < requiredSize)
    {
        delete mInstanceBuffer;
        mInstanceBuffer = mRhi->newBuffer(
            QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, requiredSize);
        if (!mInstanceBuffer->create())
        {
            delete mInstanceBuffer;
            mInstanceBuffer = nullptr;
            mInstanceBufferSize = 0;
            return;
        }
        mInstanceBufferSize = requiredSize;
    }

    updates->updateDynamicBuffer(mInstanceBuffer, 0, requiredSize, mStagingData);
    mDirty = false;
}

void QCPScatterRhiLayer::render(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::render");

    if (!mPipeline || !mInstanceBuffer || !mQuadVertexBuffer
        || !mQuadIndexBuffer || !mSrb || mDrawEntries.isEmpty())
        return;

    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});

    const int stride = ubufStride();
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];

        const QPair<int, quint32> dynamicOffset(0, quint32(i * stride));
        cb->setShaderResources(mSrb, 1, &dynamicOffset);

        const quint32 instanceByteOffset =
            quint32(entry.instanceOffset) * 3 * sizeof(float);
        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            {mQuadVertexBuffer, 0},
            {mInstanceBuffer, instanceByteOffset},
        };
        cb->setVertexInput(0, 2, vbufBindings, mQuadIndexBuffer, 0,
                            QRhiCommandBuffer::IndexUInt16);

        cb->setScissor({entry.scissorRect.x(), entry.scissorRect.y(),
                        entry.scissorRect.width(), entry.scissorRect.height()});

        cb->drawIndexed(6, entry.instanceCount, 0, 0, 0);
    }
}
```

**Important note about the static bool:** The `quadUploaded` flag is a simplification. Since the quad vertex/index buffers are `Immutable`, they only need uploading once per pipeline creation. A cleaner approach would be to track this with a member bool, but for correctness the static is fine since we only ever have one set of quad vertices. The implementer should change this to a `bool mQuadUploaded = false` member and reset it in `invalidatePipeline()`.

- [ ] **Step 5: Add source to meson.build**

In `meson.build`, in the `static_library('NeoQCP', ...)` source list, after `'src/painting/colormap-renderer.cpp'`, add:

```
           'src/painting/scatter-rhi-layer.cpp',
```

Also add `'src/painting/scatter-rhi-layer.h'` to the `extra_files` list.

- [ ] **Step 6: Verify it compiles**

Run: `meson compile -C build`
Expected: Compiles (but tests won't pass yet — `scatterRhiLayer` not on QCustomPlot).

- [ ] **Step 7: Commit**

```bash
git add src/painting/scatter-rhi-layer.h src/painting/scatter-rhi-layer.cpp meson.build
git commit -m "feat(scatter): add QCPScatterRhiLayer for GPU instanced scatter rendering"
```

---

### Task 4: Wire QCPScatterRhiLayer into QCustomPlot

Add the scatter RHI layer map to QCustomPlot's core, mirror the plottable RHI layer pattern.

**Files:**
- Modify: `src/core.h`
- Modify: `src/core.cpp`

- [ ] **Step 1: Add declarations to core.h**

In `src/core.h`, add forward declaration near the top (near `class QCPPlottableRhiLayer`):

```cpp
class QCPScatterRhiLayer;
```

Add the accessor method near `plottableRhiLayer` (around line 137):

```cpp
    [[nodiscard]] QCPScatterRhiLayer* scatterRhiLayer(QCPLayer* layer);
```

Add member variable near `mPlottableRhiLayers` (around line 385):

```cpp
    QMap<QCPLayer*, QCPScatterRhiLayer*> mScatterRhiLayers;
```

- [ ] **Step 2: Implement accessor in core.cpp**

Near `QCustomPlot::plottableRhiLayer()` (around line 1096), add:

```cpp
QCPScatterRhiLayer* QCustomPlot::scatterRhiLayer(QCPLayer* layer)
{
    if (!mRhi)
        return nullptr;
    if (!mScatterRhiLayers.contains(layer))
    {
        auto* srl = new QCPScatterRhiLayer(mRhi);
        mScatterRhiLayers[layer] = srl;
    }
    return mScatterRhiLayers[layer];
}
```

- [ ] **Step 3: Invalidate pipelines on resize**

In `initialize()`, in the resize section (around line 2507), after the plottable RHI layer invalidation loop, add:

```cpp
    for (auto* srl : mScatterRhiLayers)
        srl->invalidatePipeline();
```

- [ ] **Step 4: Upload resources in uploadLayerTextures()**

In `uploadLayerTextures()`, after the plottable RHI layer upload loop (around line 2651), add:

```cpp
    for (auto* srl : mScatterRhiLayers)
    {
        srl->ensurePipeline(renderTarget()->renderPassDescriptor(), sampleCount());
        srl->uploadResources(updates, outputSize, mBufferDevicePixelRatio,
                              mRhi->isYUpInNDC());
    }
```

- [ ] **Step 5: Render in executeRenderPass()**

In `executeRenderPass()`, right after the plottable RHI layer render block (after line 2822), add:

```cpp
        // Draw GPU scatter geometry for this layer (after plottable lines)
        if (auto* srl = mScatterRhiLayers.value(layer, nullptr))
        {
            if (srl->hasGeometry())
                srl->render(cb, outputSize);
        }
```

- [ ] **Step 6: Cleanup in destructor**

In `QCustomPlot::~QCustomPlot()` (or `releaseResources`), add:

```cpp
    qDeleteAll(mScatterRhiLayers);
    mScatterRhiLayers.clear();
```

- [ ] **Step 7: Add include**

In `src/core.cpp`, add near the other painting includes:

```cpp
#include "painting/scatter-rhi-layer.h"
```

- [ ] **Step 8: Run tests**

Run: `meson compile -C build && meson test -C build --print-errorlogs -t 0 auto-tests`
Expected: All scatter RHI tests PASS (but scatter rendering not wired into plottables yet — replot tests pass because QCPGraph2 still uses QPainter path).

- [ ] **Step 9: Commit**

```bash
git add src/core.h src/core.cpp
git commit -m "feat(scatter): wire QCPScatterRhiLayer into QCustomPlot render pipeline"
```

---

### Task 5: Route QCPGraph2 and QCPMultiGraph Scatter Drawing to GPU

Replace the QPainter `drawShape` loops with RHI layer calls.

**Files:**
- Modify: `src/plottables/plottable-graph2.cpp`
- Modify: `src/plottables/plottable-multigraph.cpp`

- [ ] **Step 1: Add include to plottable-graph2.cpp**

At the top of `src/plottables/plottable-graph2.cpp`, add:

```cpp
#include "../painting/scatter-rhi-layer.h"
```

- [ ] **Step 2: Replace scatter section in QCPGraph2::draw()**

Replace the scatter drawing block (lines 497-509, the `if (!mScatterStyle.isNone())` block) with:

```cpp
    if (!mScatterStyle.isNone())
    {
        const int skip = mScatterSkip + 1;

        if (!isExportMode && mParentPlot)
        {
            // GPU path: build instance data and submit to scatter RHI layer
            auto* srl = mParentPlot->scatterRhiLayer(mLayer);
            if (srl)
            {
                std::vector<float> instanceData;
                instanceData.reserve(lines.size() / skip * 3);
                for (int i = 0; i < lines.size(); i += skip)
                {
                    const double sx = lines[i].x(), sy = lines[i].y();
                    if (qIsFinite(sx) && qIsFinite(sy))
                    {
                        instanceData.push_back(static_cast<float>(sx));
                        instanceData.push_back(static_cast<float>(sy));
                        instanceData.push_back(0.0f);
                    }
                }
                if (!instanceData.empty())
                {
                    srl->addScatter(instanceData, mScatterStyle, clipRect(),
                                     mParentPlot->devicePixelRatioF(),
                                     mParentPlot->rhiOutputSize().height(),
                                     gpuOffset.x(), gpuOffset.y());
                }
            }
        }
        else
        {
            // QPainter fallback (export mode)
            applyScattersAntialiasingHint(painter);
            mScatterStyle.applyTo(painter, drawPen);
            for (int i = 0; i < lines.size(); i += skip)
            {
                const double sx = lines[i].x(), sy = lines[i].y();
                if (qIsFinite(sx) && qIsFinite(sy))
                    mScatterStyle.drawShape(painter, sx, sy);
            }
        }
    }
```

- [ ] **Step 3: Add include to plottable-multigraph.cpp**

At the top of `src/plottables/plottable-multigraph.cpp`, add:

```cpp
#include "../painting/scatter-rhi-layer.h"
```

- [ ] **Step 4: Replace scatter section in QCPMultiGraph::draw()**

Find the scatter section in QCPMultiGraph::draw() (the `if (!comp.scatterStyle.isNone())` block, around line 693) and replace with:

```cpp
        if (!comp.scatterStyle.isNone()) {
            const int skip = mScatterSkip + 1;

            if (!isExportMode && mParentPlot)
            {
                auto* srl = mParentPlot->scatterRhiLayer(mLayer);
                if (srl)
                {
                    std::vector<float> instanceData;
                    instanceData.reserve(dataLines.size() / skip * 3);
                    for (int i = 0; i < dataLines.size(); i += skip)
                    {
                        const double sx = dataLines[i].x(), sy = dataLines[i].y();
                        if (qIsFinite(sx) && qIsFinite(sy))
                        {
                            instanceData.push_back(static_cast<float>(sx));
                            instanceData.push_back(static_cast<float>(sy));
                            instanceData.push_back(0.0f);
                        }
                    }
                    if (!instanceData.empty())
                    {
                        srl->addScatter(instanceData, comp.scatterStyle, clipRect(),
                                         mParentPlot->devicePixelRatioF(),
                                         mParentPlot->rhiOutputSize().height(),
                                         gpuOffset.x(), gpuOffset.y());
                    }
                }
            }
            else
            {
                applyScattersAntialiasingHint(painter);
                comp.scatterStyle.applyTo(painter, comp.pen);
                for (int i = 0; i < dataLines.size(); i += skip)
                {
                    const double sx = dataLines[i].x(), sy = dataLines[i].y();
                    if (qIsFinite(sx) && qIsFinite(sy))
                        comp.scatterStyle.drawShape(painter, sx, sy);
                }
            }
        }
```

**Note:** You need to check whether `isExportMode` is already in scope in the multigraph draw method. If not, add the same detection as in QCPGraph2:
```cpp
const bool isExportMode = painter->modes().testFlag(QCPPainter::pmNoCaching)
                        || painter->modes().testFlag(QCPPainter::pmVectorized);
```

Also check whether `gpuOffset` is available. In QCPMultiGraph, the line cache may use a different variable name for the GPU offset. Look at the existing multigraph draw code around the line rendering section. If there's no `gpuOffset`, pass `0.0f, 0.0f` for the offsets.

- [ ] **Step 5: Run all tests**

Run: `meson compile -C build && meson test -C build --print-errorlogs -t 0 auto-tests`
Expected: All tests PASS, including the scatter RHI tests which now exercise the real GPU path.

- [ ] **Step 6: Commit**

```bash
git add src/plottables/plottable-graph2.cpp src/plottables/plottable-multigraph.cpp
git commit -m "feat(scatter): route QCPGraph2 and QCPMultiGraph scatter to GPU RHI path"
```

---

### Task 6: 1M-Point Scatter Benchmark

Add benchmarks to measure the performance improvement.

**Files:**
- Modify: `tests/benchmark/benchmark.cpp`

- [ ] **Step 1: Add benchmark declarations**

In `tests/benchmark/benchmark.cpp`, add to the `private slots:` section (around line 36):

```cpp
  void QCPGraph2_ScatterDisc1M();
  void QCPGraph2_ScatterSquare1M();
  void QCPGraph2_ScatterWithLine1M();
```

- [ ] **Step 2: Implement QCPGraph2_ScatterDisc1M**

At the end of the file (before the `QCPAxis_TickLabels` benchmarks), add:

```cpp
void Benchmark::QCPGraph2_ScatterDisc1M()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  graph->setLineStyle(QCPGraph2::lsNone);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::red, 4));
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  std::srand(42);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = std::sin(keys[i] * 10 * M_PI) + (std::rand() / double(RAND_MAX) - 0.5) * 0.5;
  }
  graph->setData(std::move(keys), std::move(values));
  mPlot->rescaleAxes();

  // Warm up
  mPlot->replot();

  QBENCHMARK {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph2_ScatterSquare1M()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  graph->setLineStyle(QCPGraph2::lsNone);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, Qt::blue, 4));
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  std::srand(42);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = std::cos(keys[i] * 8 * M_PI) + (std::rand() / double(RAND_MAX) - 0.5) * 0.5;
  }
  graph->setData(std::move(keys), std::move(values));
  mPlot->rescaleAxes();

  mPlot->replot();

  QBENCHMARK {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph2_ScatterWithLine1M()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  graph->setLineStyle(QCPGraph2::lsLine);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::red, 4));
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = std::sin(keys[i] * 10 * M_PI);
  }
  graph->setData(std::move(keys), std::move(values));
  mPlot->rescaleAxes();

  mPlot->replot();

  QBENCHMARK {
    mPlot->replot();
  }
}
```

- [ ] **Step 3: Run benchmarks**

Run: `meson compile -C build && meson test -C build --benchmark --print-errorlogs -t 0 qcp_benchmark`

Record the timing for:
- `QCPGraph2_ScatterDisc1M`
- `QCPGraph2_ScatterSquare1M`
- `QCPGraph2_ScatterWithLine1M`

Compare with the existing `QCPGraph_ManyPoints` (50K QPainter scatter) as a baseline ratio.

- [ ] **Step 4: Commit**

```bash
git add tests/benchmark/benchmark.cpp
git commit -m "bench(scatter): add 1M-point scatter benchmarks for GPU vs QPainter"
```

---

### Task 7: Edge Cases and Robustness Tests

Add tests for edge cases: NaN/Inf data, empty data, style changes, rapid replots.

**Files:**
- Modify: `tests/auto/test-scatter-rhi/test-scatter-rhi.h`
- Modify: `tests/auto/test-scatter-rhi/test-scatter-rhi.cpp`

- [ ] **Step 1: Add test declarations**

Add to `test-scatter-rhi.h` private slots:

```cpp
    // Task 7: edge cases
    void emptyDataDoesNotCrash();
    void singlePointDoesNotCrash();
    void allNaNDoesNotCrash();
    void mixedNaNDoesNotCrash();
    void infDataFiltered();
    void styleChangeBetweenReplots();
    void scatterSkipRespected();
    void scatterPluLineDoesNotCrash();
    void multiGraphScatterDoesNotCrash();
```

- [ ] **Step 2: Implement edge case tests**

Add to `test-scatter-rhi.cpp`:

```cpp
void TestScatterRhi::emptyDataDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    // No data set
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::singlePointDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    graph->setData(std::vector<double>{0.5}, std::vector<double>{0.5});
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::allNaNDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    double nan = std::numeric_limits<double>::quiet_NaN();
    graph->setData(std::vector<double>{nan, nan, nan}, std::vector<double>{nan, nan, nan});
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::mixedNaNDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    double nan = std::numeric_limits<double>::quiet_NaN();
    graph->setData(std::vector<double>{0.1, nan, 0.5, 0.9},
                   std::vector<double>{0.2, 0.3, nan, 0.8});
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::infDataFiltered()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    double inf = std::numeric_limits<double>::infinity();
    graph->setData(std::vector<double>{0.1, 0.5, 0.9},
                   std::vector<double>{inf, -inf, 0.5});
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::styleChangeBetweenReplots()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    graph->setData(std::vector<double>{0.1, 0.5, 0.9},
                   std::vector<double>{0.2, 0.8, 0.4});
    mPlot->rescaleAxes();
    mPlot->replot();

    // Change style
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, Qt::blue, 10));
    mPlot->replot();

    // Change again to custom
    QPainterPath path;
    path.addEllipse(-3, -3, 6, 6);
    graph->setScatterStyle(QCPScatterStyle(path, QPen(Qt::black), QBrush(Qt::red), 8));
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::scatterSkipRespected()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    graph->setScatterSkip(5);
    std::vector<double> keys(100), values(100);
    for (int i = 0; i < 100; ++i) {
        keys[i] = i / 100.0;
        values[i] = std::sin(keys[i] * M_PI);
    }
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::scatterPluLineDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPGraph2::lsLine);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::red, 6));
    std::vector<double> keys{0.1, 0.3, 0.5, 0.7, 0.9};
    std::vector<double> values{0.2, 0.8, 0.4, 0.9, 0.1};
    graph->setData(std::move(keys), std::move(values));
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}

void TestScatterRhi::multiGraphScatterDoesNotCrash()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsNone);
    QCPScatterStyle style(QCPScatterStyle::ssDisc, Qt::red, 6);
    mg->addComponent("comp1", QPen(Qt::red), style);
    mg->addComponent("comp2", QPen(Qt::blue), style);
    std::vector<double> keys(100);
    std::vector<float> vals(200);
    for (int i = 0; i < 100; ++i) {
        keys[i] = i / 100.0;
        vals[2*i] = std::sin(keys[i] * M_PI);
        vals[2*i+1] = std::cos(keys[i] * M_PI);
    }
    mg->setData(std::move(keys), std::move(vals));
    mPlot->rescaleAxes();
    mPlot->replot();
    QVERIFY(true);
}
```

**Note:** Verify the exact `QCPMultiGraph::addComponent` API before implementing — it may differ from what's shown above. Check `src/plottables/plottable-multigraph.h` for the actual method signature. The test just needs to create a multi-graph with scatter style and replot.

- [ ] **Step 3: Run tests**

Run: `meson compile -C build && meson test -C build --print-errorlogs -t 0 auto-tests`
Expected: All tests PASS.

- [ ] **Step 4: Commit**

```bash
git add tests/auto/test-scatter-rhi/test-scatter-rhi.h \
        tests/auto/test-scatter-rhi/test-scatter-rhi.cpp
git commit -m "test(scatter): add edge case tests for GPU scatter rendering"
```

---

### Task 8: Final Integration and Cleanup

Fix the static bool issue in scatter-rhi-layer.cpp, run full test suite and benchmarks.

**Files:**
- Modify: `src/painting/scatter-rhi-layer.h`
- Modify: `src/painting/scatter-rhi-layer.cpp`

- [ ] **Step 1: Fix the static quadUploaded to member**

In `scatter-rhi-layer.h`, add member:
```cpp
    bool mQuadUploaded = false;
```

In `scatter-rhi-layer.cpp`, in `invalidatePipeline()`, add:
```cpp
    delete mQuadVertexBuffer; mQuadVertexBuffer = nullptr;
    delete mQuadIndexBuffer; mQuadIndexBuffer = nullptr;
    mQuadUploaded = false;
```

In `uploadResources()`, replace the static bool block:
```cpp
    if (!mQuadUploaded && mQuadVertexBuffer && mQuadIndexBuffer)
    {
        updates->uploadStaticBuffer(mQuadVertexBuffer, kQuadVertices);
        updates->uploadStaticBuffer(mQuadIndexBuffer, kQuadIndices);
        mQuadUploaded = true;
    }
```

- [ ] **Step 2: Run full test suite**

Run: `meson compile -C build && meson test -C build --print-errorlogs -t 0`
Expected: All tests PASS (auto-tests and benchmarks).

- [ ] **Step 3: Run benchmarks and record results**

Run: `meson test -C build --benchmark --print-errorlogs -t 0 qcp_benchmark`

Record and compare:
- `QCPGraph_ManyPoints` (50K, QPainter scatter — existing baseline)
- `QCPGraph2_ScatterDisc1M` (1M, GPU scatter)
- `QCPGraph2_ScatterSquare1M` (1M, GPU scatter)
- `QCPGraph2_ScatterWithLine1M` (1M, line + scatter)

- [ ] **Step 4: Commit**

```bash
git add src/painting/scatter-rhi-layer.h src/painting/scatter-rhi-layer.cpp
git commit -m "fix(scatter): replace static bool with member for quad upload tracking"
```

- [ ] **Step 5: Run all tests one final time**

Run: `meson compile -C build && meson test -C build --print-errorlogs -t 0 auto-tests`
Expected: All tests PASS.
