VibeNote Agent Specification

This document instructs a Codex agent on how to build the VibeNote project from scratch. It describes every directory and file that must be created, how each component communicates with the others, and prescribes strict system‑level constraints (Wayland‑only, Zen kernel, NVML, etc.). Use this blueprint as the single source of truth when generating code; do not invent any new files or behaviours outside this schema. Follow the do/don’t guidelines exactly and ensure the final repository matches this structure and behaviour.
Global guidelines

    System constraints: VibeNote runs on Arch Linux with the Zen kernel, pure Wayland (KWin/Plasma), and an NVIDIA RTX 3050 Ti using the nvidia‑open‑dkms driver. Never depend on X11, xrandr, xclip, xsel or any X11 tools. For display control use wlr‑randr. For clipboard operations use wl‑copy/wl‑paste. For audio and screen capture use PipeWire and xdg‑desktop‑portal; do not suggest PulseAudio or X11 screen‑capture utilities. Use NVML for GPU telemetry; never use nouveau or non‑DKMS drivers.

    Languages & tooling: The GUI is written in C++20 and QML using Qt 6 and Kirigami. The daemon is C++20. Build with CMake presets and Ninja; follow warnings‑as‑errors and use the provided toolchains. SQLite stores notes; SQL schema is versioned in daemon/src/store/schema.sql. Use ONNX Runtime only for optional OCR acceleration (PaddleOCR). All HTTP endpoints must conform to the openapi.yaml. Use Prometheus exposition for metrics. Unit tests live under tests/ and run via CTest. Enforce the .clang-format and .clang-tidy rules.

    Sandbox & security: The daemon listens only on localhost and must never bind to external interfaces. Concurrency is controlled via a priority queue; never dispatch more jobs than the GPU can handle. Use NVML to cap GPU utilisation and, when threshold is exceeded, delay further requests. Watch mode (OCR capture) must be opt‑in and respect user privacy. Never transmit data off the machine unless the user provides external API keys; even then, restrict calls to the allowed enrichment endpoints.

    Do not: Don’t rely on X11 libraries or tools. Don’t suggest installing non‑DKMS Nvidia drivers. Don’t embed secrets or API keys in the code. Don’t block the GUI thread when waiting on the daemon; all API calls must be asynchronous via signals/slots. Don’t use blocking IO in the daemon’s HTTP handlers; use appropriate thread pools.

Agent tasks

    Create the directory tree as defined under structure in the JSON below. Each directory must exist; create all listed files with their names and relative paths.

    Populate each file according to its description and implementation fields. The implementation guidance may reference other modules that must be created concurrently. If a file has an interacts_with array, ensure its implementation calls into, imports, or otherwise cooperates with the referenced files.

    Adhere to interactions. The interacts_with entries define explicit communication channels—function calls, API requests, SQL queries or data flow. Do not add undeclared dependencies.

    Follow the DO/DO NOT sections (global guidelines) when coding. Respect Wayland‑only constraints and GPU/CPU balancing semantics. Use asynchronous patterns and ensure thread‑safety.

    Generate CMake files with proper linking. Link each module to Qt6 modules, KF6 libraries, NVML, SQLite and other libraries as needed. Respect the cmake/ helpers and use them correctly.

    Implement tests under tests/. Use mocks for NVML, PipeWire and external APIs. Integration tests must simulate watch mode, queue behaviour, export flows and metric exposure.

    Provide no extra files. Only the files declared in this schema should be present. Any auto‑generated files (like .kcfgc) will be produced by the build system and should be ignored by version control.

JSON schema

The following JSON object describes the entire repository layout. Each element has:

    path: relative path from the repository root.

    type: either directory or file.

    description: a human‑readable explanation of the purpose of this file or directory.

    interacts_with: a list of other files (within or outside this folder) that this file depends upon or communicates with. Use these to wire up imports, function calls, HTTP requests, logging, etc.

    implementation (files only): prescriptive guidance on what to implement in this file—classes, functions, logic, or configuration. It may reference system constraints. If implementation is omitted, the description alone is sufficient and the file likely contains static data (e.g. schema.sql, openapi.yaml, CMake helpers).

{
  "version": "1.0",
  "repository": "VibeNote",
  "structure": [
    {
      "path": "app/",
      "type": "directory",
      "description": "Kirigami‑based GUI and overlay application that provides real‑time interaction with the local summariser.",
      "children": [
        {
          "path": "app/qml/",
          "type": "directory",
          "description": "QML source files defining UI components, pages and themes for the overlay, dashboard, settings and integrations.",
          "children": []
        },
        {
          "path": "app/icons/",
          "type": "directory",
          "description": "Icon assets used by the UI (SVG/PNG).  Store only static assets; no code resides here.",
          "children": []
        },
        {
          "path": "app/src/",
          "type": "directory",
          "description": "C++ sources implementing the GUI logic, API client, settings store and metrics view.",
          "children": [
            {
              "path": "app/src/main.cpp",
              "type": "file",
              "description": "Entry point for the GUI.  Instantiates `QGuiApplication`, configures `KAboutData`, registers `SettingsStore` and `ApiClient` singletons for QML, sets up the global hotkey via `KGlobalAccel`, and loads `Main.qml` from the resource file.",
              "interacts_with": [
                "app/src/overlay_controller.cpp",
                "app/src/api_client.cpp",
                "app/src/settings_store.cpp",
                "app/qml/Main.qml"
              ],
              "implementation": "Create a `QGuiApplication`, call `KLocalizedString::init`, populate `KAboutData` with application metadata, then register the SettingsStore and ApiClient types using `qmlRegisterSingletonInstance`.  Bind the Ctrl+Alt+Space hotkey via KGlobalAccel to a slot in OverlayController.  Finally, load `Main.qml` from the compiled Qt resource system.  Do not block the event loop; all heavy operations must be delegated to the daemon via ApiClient."
            },
            {
              "path": "app/src/overlay_controller.cpp",
              "type": "file",
              "description": "Implements the global hotkey overlay behaviour.  Handles showing and hiding the overlay, capturing user input, sending queries to the daemon and displaying responses.",
              "interacts_with": [
                "app/src/api_client.cpp",
                "app/qml/Overlay.qml",
                "daemon/src/http_server.cpp"
              ],
              "implementation": "Define a QObject‑derived class that exposes `showOverlay()` and `hideOverlay()` slots.  Connect the `QHotkey` (or KF6 KGlobalAccel) to toggle the overlay.  When the user submits text, emit a signal that triggers ApiClient to call `/v1/chat/completions` or `/v1/summarize`.  Handle asynchronous responses by updating bound QML properties.  Ensure the overlay is non‑modal, semi‑transparent, and closes on escape key."
            },
            {
              "path": "app/src/settings_store.cpp",
              "type": "file",
              "description": "Manages persistent application settings (e.g. theme, watch mode toggle, export preferences and API keys).  Persists to `~/.config/VibeNote/config.yml` using KConfig or QSettings and exposes properties to QML.",
              "interacts_with": [
                "daemon/src/config.cpp",
                "app/qml/settings pages"
              ],
              "implementation": "Use KF6’s `KConfig` or Qt’s `QSettings` to load and store settings.  Provide Q_PROPERTY accessors for each configurable option (boolean watchModeEnabled, double gpuThreshold, QString apiKey1, apiKey2, QStringList exportFormats, etc.).  Connect property changes to signals so that QML can react.  When settings change that affect the daemon (e.g. concurrency limits, GPU thresholds), send updates via ApiClient to `/v1/config`.  Ensure sensitive fields (API keys) are stored using `QKeychain` and not written in plain text."
            },
            {
              "path": "app/src/api_client.cpp",
              "type": "file",
              "description": "Thin HTTP client wrapper around the daemon’s REST API.  Uses `QNetworkAccessManager` to call endpoints defined in `openapi.yaml` and emits signals for the GUI.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/openapi.yaml",
                "app/src/overlay_controller.cpp",
                "app/src/metrics_view.cpp"
              ],
              "implementation": "Encapsulate a `QNetworkAccessManager` and provide asynchronous methods such as `status()`, `summarize(prompt)`, `fetchNotes(range)`, `exportNotes(format, range)`, `toggleWatchMode(bool)`, and `updateConfig(json)`.  Each method constructs an HTTP request to `http://127.0.0.1:<daemon_port>` following the OpenAPI specification.  Parse JSON responses using `QJsonDocument` and emit signals for success or error.  Use appropriate timeouts and handle connection errors gracefully.  Do not block the GUI thread; use signals/slots."
            },
            {
              "path": "app/src/metrics_view.cpp",
              "type": "file",
              "description": "Retrieves and aggregates Prometheus metrics from the daemon (queue depth, GPU usage, note rate) and exposes them to the dashboard QML for chart rendering.",
              "interacts_with": [
                "daemon/src/metrics.cpp",
                "app/src/api_client.cpp",
                "app/qml/DashboardPage.qml"
              ],
              "implementation": "Implement a class that periodically calls `ApiClient::fetchMetrics()` to `/metrics` endpoint and parses the Prometheus exposition format.  Aggregate series by category (e.g. `vibenote_notes_total`, `vibenote_gpu_utilization`, `vibenote_queue_length`) and store them in C++ containers.  Expose QML properties for current values and history so that charts can be drawn.  Provide helper functions to reset or clear metrics (e.g. when a new export is started)."
            }
          ]
        },
        {
          "path": "app/app.qrc",
          "type": "file",
          "description": "Qt resource collection file enumerating all QML and icon assets.  Ensures they are compiled into the binary so that paths like `qrc:/Main.qml` work.",
          "interacts_with": [
            "app/qml/*",
            "app/icons/*"
          ],
          "implementation": "List each QML file under `<file>` tags and each icon under `<file>`.  Do not omit any asset; otherwise the application may fail to load QML at runtime.  This file is consumed by CMake’s `qt_add_resources()`."
        },
        {
          "path": "app/CMakeLists.txt",
          "type": "file",
          "description": "CMake build rules for the GUI.  Defines a static library for shared sources and an executable, links against Qt6 modules (Core, Gui, Quick, QuickControls2, Network), KF6 components (I18n, Config, Kirigami, GlobalAccel), and includes the generated resources.",
          "interacts_with": [
            "CMakePresets.json",
            "cmake/toolchains.cmake"
          ],
          "implementation": "Use `qt_standard_project_setup()` to set defaults.  Call `qt_add_executable(vibenote_app ...)` with the sources listed above.  Use `target_link_libraries()` to link Qt6::Core, Qt6::Gui, Qt6::Qml, Qt6::Quick, Qt6::QuickControls2, Qt6::Network, KF6::I18n, KF6::ConfigWidgets, KF6::KirigamiAddons, KF6::GlobalAccel.  Include `app/app.qrc` via `qt_add_resources`.  Install the binary and `.desktop` file with appropriate paths.  Respect the warnings configuration and treat warnings as errors."
        },
        {
          "path": "app/org.saphyre.VibeNote.desktop",
          "type": "file",
          "description": "XDG desktop entry for launching VibeNote.  Registers the executable and the `Ctrl+Alt+Space` global hotkey via KDE services.",
          "interacts_with": [
            "app/src/main.cpp"
          ],
          "implementation": "Define a `[Desktop Entry]` with `Exec=vibenote_app`, `Name=VibeNote`, `Comment=Local summariser and activity tracker`, `Icon=org.saphyre.VibeNote`, `Categories=Utility;`, and `X-KDE-GlobalAccel=Ctrl+Alt+Space`.  Install it to the correct XDG applications directory via CMake."
        }
      ]
    },
    {
      "path": "daemon/",
      "type": "directory",
      "description": "Daemon implementing the localhost HTTP API, task queue, GPU guard, OCR pipeline, window watcher, persistence, enrichment and export modules.",
      "children": [
        {
          "path": "daemon/src/",
          "type": "directory",
          "description": "C++ sources for the daemon.  Each file is responsible for a separate subsystem.  The main entrypoint assembles them and starts the event loop.",
          "children": [
            {
              "path": "daemon/src/main.cpp",
              "type": "file",
              "description": "Entrypoint for the daemon.  Parses config, initializes NVML, spawns the llama.cpp server (if not already running), constructs the queue, gpu_guard, http_server, capture and watcher modules and enters the Qt event loop.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/src/queue.cpp",
                "daemon/src/gpu_guard.cpp",
                "daemon/src/capture/screencast_portal.cpp",
                "daemon/src/windows/kwin_watcher.cpp",
                "daemon/src/ocr/ocr_engine.h",
                "daemon/src/store/sqlite_store.cpp",
                "daemon/src/enrich/enrich_none.cpp",
                "daemon/openapi.yaml",
                "third_party/llama.cpp/server"
              ],
              "implementation": "Parse command‑line arguments or read configuration via `config.cpp`.  Call `nvmlInit_v2()` and allocate a `GpuGuard` instance.  Start the llama.cpp server (via `llama_client.cpp`) if `--spawn-server` is specified; otherwise assume it is launched separately.  Construct `TaskQueue`, `HttpServer`, `ScreenCapture` and `WindowWatcher` objects and connect their signals/slots.  Start a QThread for the capture pipeline and OCR processing.  Register a cleanup routine to shut down NVML and the llama server on exit.  Run the Qt event loop using `QCoreApplication`."
            },
            {
              "path": "daemon/src/http_server.cpp",
              "type": "file",
              "description": "Implements the REST API using Qt’s `QHttpServer` (or `QHttpServer` equivalent).  Routes requests to handlers, validates input against the OpenAPI schema, enqueues summarisation jobs, toggles watch mode and returns results.",
              "interacts_with": [
                "daemon/openapi.yaml",
                "daemon/src/queue.cpp",
                "daemon/src/llama_client.cpp",
                "daemon/src/store/sqlite_store.cpp",
                "daemon/src/exporters/export_raw.cpp",
                "daemon/src/exporters/export_json.cpp",
                "daemon/src/exporters/export_csv.cpp",
                "daemon/src/enrich/enrich_openai.cpp",
                "daemon/src/enrich/enrich_secondary.cpp",
                "daemon/src/metrics.cpp",
                "daemon/src/config.cpp"
              ],
              "implementation": "Construct an `QHttpServer` instance bound to 127.0.0.1.  Use lambda handlers or dedicated functions for each endpoint: `/v1/status` returns health info (queue depth, GPU load, model state); `/v1/summarize` accepts JSON with `prompt` and optional parameters, enqueues a job to the TaskQueue and streams the summarised note when ready; `/v1/notes` returns stored notes from SQLite; `/v1/export` triggers export via the exporters; `/v1/watch/start` and `/v1/watch/stop` toggle the OCR pipeline; `/v1/config` reads/updates runtime configuration via Config; `/metrics` returns Prometheus metrics.  Validate inputs according to openapi.yaml; return 400 or 429 as appropriate.  Use asynchronous calls to LlamaClient and do not block the HTTP worker threads."
            },
            {
              "path": "daemon/src/queue.cpp",
              "type": "file",
              "description": "Implements a priority queue and concurrency controller.  Holds multiple classes of tasks—system watch (OCR), interactive API requests, and bulk exports.  Enforces per‑class concurrency limits and coordinates with the GPU guard to throttle jobs when resources are low.",
              "interacts_with": [
                "daemon/src/gpu_guard.cpp",
                "daemon/src/http_server.cpp",
                "daemon/src/llama_client.cpp"
              ],
              "implementation": "Define an enum for classes (Watch, Interactive, Export) and maintain separate FIFOs.  Use a mutex and condition variables to dequeue tasks safely.  When `GpuGuard::canRun()` returns false, hold new tasks until GPU utilisation drops below the threshold.  Provide functions `enqueue(Task)` and `dequeue()` called by HttpServer handlers.  Ensure fairness between classes (e.g. allow one export job at a time while prioritising interactive requests).  Document concurrency settings in config."
            },
            {
              "path": "daemon/src/gpu_guard.cpp",
              "type": "file",
              "description": "Monitors GPU utilisation and VRAM usage via NVML.  Sends signals to the queue when resources are constrained and restarts the llama.cpp server with adjusted GPU offload layers when thresholds change.",
              "interacts_with": [
                "daemon/src/queue.cpp",
                "daemon/src/llama_client.cpp",
                "third_party/llama.cpp/server"
              ],
              "implementation": "Periodically call `nvmlDeviceGetUtilizationRates()` and `nvmlDeviceGetMemoryInfo()`.  Compute a moving average of GPU utilisation and compare against a configurable threshold (default 80%).  If utilisation exceeds the threshold, emit a signal that pauses the queue.  When utilisation drops, resume.  Provide a function to determine the optimal `-ngl` GPU offload parameter based on available VRAM and restart the llama server if necessary (via LlamaClient).  Ensure NVML is initialised and shut down properly."
            },
            {
              "path": "daemon/src/llama_client.cpp",
              "type": "file",
              "description": "Manages the TCP connection to the llama.cpp server.  Sends prompts, controls streaming and handles cancellations.  Used by API handlers and enrichment modules.",
              "interacts_with": [
                "third_party/llama.cpp/server",
                "daemon/src/http_server.cpp",
                "daemon/src/enrich/enrich_openai.cpp"
              ],
              "implementation": "Implement a class with methods `connect()`, `disconnect()`, `streamCompletion(prompt, params)` and `stopSequence(id)`.  Use a QTcpSocket (or low‑level POSIX sockets) to send OpenAI‑style completion requests to the llama server.  Support both streaming and non‑streaming responses.  Ensure thread‑safety when used concurrently by multiple tasks.  Provide status signals (connected, disconnected, error) for other modules."
            },
            {
              "path": "daemon/src/ocr/ocr_engine.h",
              "type": "file",
              "description": "Abstract base class defining the interface for OCR backends.  Declares methods to initialise, process image regions and shut down.",
              "interacts_with": [
                "daemon/src/ocr/ocr_tesseract.cpp",
                "daemon/src/ocr/ocr_paddle.cpp",
                "daemon/src/capture/frame_diff.cpp"
              ],
              "implementation": "Define a pure virtual class `OcrEngine` with methods `bool init()`, `std::vector<OcrSpan> infer(const QImage &frame, const std::vector<QRect> &regions)`, and `void shutdown()`.  `OcrSpan` should contain bounding box, text and confidence.  Provide a factory function to instantiate either `OcrTesseract` or `OcrPaddle` based on configuration."
            },
            {
              "path": "daemon/src/ocr/ocr_tesseract.cpp",
              "type": "file",
              "description": "CPU‑based OCR implementation using Tesseract.  Processes greyscale QImages and returns a vector of OcrSpan.",
              "interacts_with": [
                "daemon/src/ocr/ocr_engine.h",
                "daemon/src/capture/frame_diff.cpp"
              ],
              "implementation": "Link against libtesseract and libleptonica.  In `init()`, instantiate a Tesseract API object, set language to `eng` and configure page segmentation mode (PSM).  In `infer()`, for each region, convert the QImage portion to a Leptonica PIX and call `TessBaseAPI::SetImage`.  Extract text and bounding boxes.  Return a list of OcrSpan objects.  Release memory properly."
            },
            {
              "path": "daemon/src/ocr/ocr_paddle.cpp",
              "type": "file",
              "description": "Optional GPU‑accelerated OCR implementation using PaddleOCR via ONNX Runtime.  Enabled only if compiled with CUDA and ONNX; falls back to Tesseract otherwise.",
              "interacts_with": [
                "daemon/src/ocr/ocr_engine.h",
                "daemon/src/capture/frame_diff.cpp"
              ],
              "implementation": "Use ONNX Runtime C++ API with the CUDA execution provider.  Load the PaddleOCR model (CRNN‑CTC for English) from the models directory.  Preprocess QImage regions into the expected tensor shape, run the session and decode the output to text.  Provide comparable interfaces to `OcrTesseract`.  If ONNX or CUDA is not available, disable this module in CMake via a flag and fall back to OcrTesseract."
            },
            {
              "path": "daemon/src/capture/screencast_portal.cpp",
              "type": "file",
              "description": "Manages Wayland screen capture via xdg‑desktop‑portal and PipeWire.  Requests a capture session, maps DMA buffers to CPU memory and emits frames for OCR.",
              "interacts_with": [
                "daemon/src/capture/frame_diff.cpp",
                "daemon/src/ocr/ocr_engine.h"
              ],
              "implementation": "Use xdg‑desktop‑portal’s DBus API to create a `ScreenCast` session.  Bind to PipeWire using libspa or GStreamer to consume frames.  Convert DMA‑buffer frames into QImages and emit them via a signal.  If the portal denies access, retry gracefully or disable watch mode.  Do not block the main thread; run in a QThread."
            },
            {
              "path": "daemon/src/capture/frame_diff.cpp",
              "type": "file",
              "description": "Computes frame differences and bounding boxes to reduce the number of regions sent to OCR.  Uses a simple threshold on pixel difference and merges nearby bounding boxes.",
              "interacts_with": [
                "daemon/src/capture/screencast_portal.cpp",
                "daemon/src/ocr/ocr_engine.h",
                "daemon/src/windows/kwin_watcher.cpp"
              ],
              "implementation": "Maintain a rolling reference frame.  On each new frame, compute absolute difference, apply a binary threshold and use connected‑component analysis to find changed regions.  Merge overlapping regions and ignore small noise.  Pass the resulting bounding boxes to the active OcrEngine.  Provide functions to update the reference frame (e.g. when the active window changes via KWinWatcher)."
            },
            {
              "path": "daemon/src/windows/kwin_watcher.cpp",
              "type": "file",
              "description": "Listens to KWin DBus signals to monitor the active window.  Provides metadata such as window title, application identifier and PID.  Important for note context and watch mode filtering.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/src/capture/frame_diff.cpp",
                "daemon/src/store/sqlite_store.cpp"
              ],
              "implementation": "Connect to the org.kde.kwin DBus interface.  Listen for `windowActivated`, `windowTitleChanged` and `windowRemoved` signals.  When the active window changes, update global state with the new title and app ID.  Provide a method to query the current window info (title, PID, app ID) for note generation and storage."
            },
            {
              "path": "daemon/src/store/sqlite_store.cpp",
              "type": "file",
              "description": "Wraps SQLite access for notes and metadata.  Provides functions to insert notes, retrieve them by time range, store window events and query aggregated metrics.  Ensures WAL mode and FTS indexing.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/src/exporters/export_raw.cpp",
                "daemon/src/exporters/export_json.cpp",
                "daemon/src/exporters/export_csv.cpp",
                "daemon/src/metrics.cpp"
              ],
              "implementation": "Use the SQLite C API or C++ wrapper (e.g. SQLiteCpp) to open a database file in the user’s data directory (e.g. `~/.local/share/VibeNote/vibenote.db`).  Ensure `PRAGMA journal_mode=WAL` and `PRAGMA synchronous=NORMAL`.  Create tables for `notes`, `windows`, `events` and an FTS5 virtual table for full‑text search.  Implement prepared statements for insertion and queries.  Write migrations in `schema.sql`.  Expose high‑level functions `insertNote(Note)`, `queryNotes(start, end)`, `insertEvent(Event)`, `getStats(...)`, etc.  Use mutexes to guard concurrent access."
            },
            {
              "path": "daemon/src/store/schema.sql",
              "type": "file",
              "description": "SQL schema used by `sqlite_store.cpp` to initialise tables and indexes.  Contains FTS5 virtual table definitions and migration scripts.",
              "interacts_with": [
                "daemon/src/store/sqlite_store.cpp"
              ],
              "implementation": "Write `CREATE TABLE` statements for `notes`, `windows`, `events`, `metrics`, `enriched_notes`, along with `CREATE VIRTUAL TABLE notes_fts USING fts5(content, tokenize='unicode61')`.  Add triggers to keep FTS synced.  Include version numbers and upgrade path."
            },
            {
              "path": "daemon/src/exporters/export_raw.cpp",
              "type": "file",
              "description": "Streams notes as newline‑delimited JSON (NDJSON) for bulk export via `/v1/export?format=raw`.  Supports date filtering and chunked responses.",
              "interacts_with": [
                "daemon/src/store/sqlite_store.cpp",
                "daemon/src/http_server.cpp"
              ],
              "implementation": "Implement a function `exportRaw(start, end, stream)` that queries notes from SQLite in chronological order and writes each as a JSON object followed by a newline.  For large exports, split into HTTP chunks (e.g. 1 MB).  Use `QJsonDocument` for serialisation.  Ensure the client can resume if interrupted (e.g. using a `cursor` parameter)."
            },
            {
              "path": "daemon/src/exporters/export_json.cpp",
              "type": "file",
              "description": "Exports notes as a JSON array conforming to the canonical blueprint schema.  Validates the output schema and includes a manifest with checksums.",
              "interacts_with": [
                "daemon/src/store/sqlite_store.cpp",
                "daemon/src/http_server.cpp"
              ],
              "implementation": "Implement a function `exportJson(start, end, stream)` that constructs a JSON object with a `manifest` (version, generated_at, count, checksum) and a `notes` array.  For each note, include fields `timestamp`, `window`, `summary`, `enriched_summary`, `metrics`.  Validate against the blueprint JSON schema using a JSON Schema library before streaming.  Compute a SHA‑256 checksum of the note content and include in the manifest.  Support chunked transfer encoding."
            },
            {
              "path": "daemon/src/exporters/export_csv.cpp",
              "type": "file",
              "description": "Exports notes as CSV files.  Provides both raw CSV and structured prompts (question/answer) formats depending on the requested `format` parameter.",
              "interacts_with": [
                "daemon/src/store/sqlite_store.cpp",
                "daemon/src/http_server.cpp"
              ],
              "implementation": "Implement two functions: `exportCsv(start, end, stream)` and `exportStructuredPrompts(start, end, stream)`.  The raw CSV should include columns for `timestamp`, `window_title`, `app_id`, `summary`, `enriched_summary`, `durations`, etc., and respect RFC4180 (comma separation, quotes and newlines).  The structured prompts CSV should, for each note, generate AI‑ready question/answer pairs from the note content (e.g. question: 'What was I doing at 15:34?', answer: note.summary).  Use a library like `fmt` or manual string formatting.  Support splitting output into multiple files when size exceeds 10 MiB."
            },
            {
              "path": "daemon/src/enrich/enrich_none.cpp",
              "type": "file",
              "description": "No‑operation enrichment module.  If no external API keys are provided, this module leaves the note unchanged and returns immediately.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/src/llama_client.cpp"
              ],
              "implementation": "Implement a function `enrich(note)` that simply returns the input note.  Provide a factory that returns this implementation when no API keys are configured."
            },
            {
              "path": "daemon/src/enrich/enrich_openai.cpp",
              "type": "file",
              "description": "Enrichment module that sends initial note summaries to an external provider (e.g. OpenAI) and returns refined summaries.  Uses API Key #1 from the configuration.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/src/llama_client.cpp"
              ],
              "implementation": "Use QtNetwork or a third‑party HTTP client to call the OpenAI API.  Construct a prompt combining the original summary and context (window title, timestamps) and post to the provider.  Respect rate limits and timeouts.  Implement exponential backoff and caching to avoid duplicate calls.  Store the enriched summary back into SQLite via `sqlite_store.cpp`.  Only activate if API Key #1 is provided; otherwise fall back to enrich_none."
            },
            {
              "path": "daemon/src/enrich/enrich_secondary.cpp",
              "type": "file",
              "description": "Optional second enrichment stage that processes aggregated metrics to produce natural‑language analyses for the dashboard (e.g. 'You spent 30% of your time coding in Rust this week').  Uses API Key #2.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/src/llama_client.cpp",
                "daemon/src/store/sqlite_store.cpp"
              ],
              "implementation": "Define a function `enrichMetrics(metrics)` that sends aggregated statistics (like total time per language, website or window) to the external provider and retrieves a human‑readable narrative.  Store the narrative in a separate table or return it to the caller via the `/metrics` endpoint.  Only used when API Key #2 is present."
            },
            {
              "path": "daemon/src/config.cpp",
              "type": "file",
              "description": "Loads and validates YAML configuration from `~/.config/VibeNote/config.yml`.  Provides getters for queue limits, GPU thresholds, OCR backend selection, API keys and export preferences.  Supports live reloading via the `/v1/config` endpoint.",
              "interacts_with": [
                "daemon/src/main.cpp",
                "daemon/src/http_server.cpp",
                "app/src/settings_store.cpp"
              ],
              "implementation": "Use libyaml or YAML‑CPP to parse the configuration file.  Provide default values.  Validate numeric ranges (e.g. concurrency >= 1, GPU threshold between 0 and 100).  Expose getters and a `reload()` function.  Connect reload to a file watcher and to the `/v1/config` handler.  Write changes back to disk when updated via the API."
            },
            {
              "path": "daemon/src/logging.cpp",
              "type": "file",
              "description": "Provides structured JSON logging to stdout/stderr.  Each module should use this helper to emit timestamped events with context and severity.",
              "interacts_with": [
                "daemon/src/main.cpp",
                "daemon/src/http_server.cpp",
                "daemon/src/queue.cpp"
              ],
              "implementation": "Define functions like `logInfo(component, message, context)`, `logWarn(...)`, `logError(...)` that write JSON lines to stderr.  Each entry should include ISO8601 timestamp, component name, severity and any key‑value pairs in context.  Use `std::chrono` for timestamps.  Provide macros to ease use throughout the codebase."
            },
            {
              "path": "daemon/src/metrics.cpp",
              "type": "file",
              "description": "Collects internal metrics and exposes them via the `/metrics` endpoint in Prometheus text format.  Tracks note throughput, queue length, GPU utilisation, OCR frame rate and other diagnostics.",
              "interacts_with": [
                "daemon/src/http_server.cpp",
                "daemon/src/queue.cpp",
                "daemon/src/gpu_guard.cpp",
                "daemon/src/store/sqlite_store.cpp"
              ],
              "implementation": "Define a singleton or namespace that holds counters, gauges and histograms.  Increment counters on each note processed, queue operation and export.  Update gauges from GpuGuard and Queue.  Provide a function `toPrometheus()` that returns a string in Prometheus exposition format.  Expose a method to reset metrics (useful for tests)."
            }
          ]
        },
        {
          "path": "daemon/include/",
          "type": "directory",
          "description": "Public header files exposing the daemon’s interfaces for unit tests and potential extension libraries.  Keep this minimal; most headers can remain in `src/`.",
          "children": []
        },
        {
          "path": "daemon/CMakeLists.txt",
          "type": "file",
          "description": "CMake rules to build the daemon.  Defines the executable, sets compiler flags, links against Qt6, NVML, SQLite, ONNX Runtime, PipeWire and other libraries, and installs `openapi.yaml`.  Supports options for enabling/disabling GPU OCR.",
          "interacts_with": [
            "cmake/toolchains.cmake",
            "third_party/llama.cpp"
          ],
          "implementation": "Use `qt_standard_project_setup()` and define an executable target `vibenote_daemon` with the source files listed above.  Use `target_link_libraries()` to link Qt6::Core, Qt6::Network, Qt6::DBus, NVML, SQLite, ONNXRuntime (if GPU OCR is enabled), PipeWire and libportal.  Use `find_package(Qt6 REQUIRED COMPONENTS ...)` accordingly.  Create an option `ENABLE_PADDLE_OCR` that adds `ocr_paddle.cpp` and links ONNX.  Install `openapi.yaml` into a share directory."
        },
        {
          "path": "daemon/openapi.yaml",
          "type": "file",
          "description": "OpenAPI 3.1 specification describing all REST endpoints (status, summarize, notes, export, watch, config, metrics).  Defines request/response schemas and error codes.",
          "interacts_with": [
            "daemon/src/http_server.cpp",
            "app/src/api_client.cpp"
          ],
          "implementation": "Write a complete OpenAPI document with paths `/v1/status`, `/v1/summarize`, `/v1/notes`, `/v1/export`, `/v1/watch/start`, `/v1/watch/stop`, `/v1/config` and `/metrics`.  Each path should define HTTP methods, request bodies, responses, error codes, and security (local only).  Use components to define shared schemas for notes, summaries, errors and exports."
        }
      ]
    },
    {
      "path": "third_party/",
      "type": "directory",
      "description": "External dependencies included as submodules or vendored code.",
      "children": [
        {
          "path": "third_party/llama.cpp/",
          "type": "directory",
          "description": "Pinned clone of ggerganov/llama.cpp.  Build this as part of the daemon using the `scripts/setup-arch.sh` or `scripts/build.sh`.",
          "children": []
        }
      ]
    },
    {
      "path": "models/",
      "type": "directory",
      "description": "Directory to store GGUF model weights for the summariser (e.g. Qwen2.5‑3B).  You must download the model via a script and place it here before running the daemon.",
      "children": []
    },
    {
      "path": "cmake/",
      "type": "directory",
      "description": "Common CMake toolchains and warning flags used across the project.",
      "children": [
        {
          "path": "cmake/toolchains.cmake",
          "type": "file",
          "description": "Defines compiler and toolchain settings for cross‑platform builds.  Sets C++ standard to 20 and configures warnings as errors.",
          "interacts_with": []
        },
        {
          "path": "cmake/warnings.cmake",
          "type": "file",
          "description": "Lists compiler warnings and adds them as errors for project sources.  Include this in each CMakeLists.",
          "interacts_with": []
        }
      ]
    },
    {
      "path": "packaging/systemd/",
      "type": "directory",
      "description": "Systemd unit files to run the local model and daemon as user services.",
      "children": [
        {
          "path": "packaging/systemd/vibemodel.service",
          "type": "file",
          "description": "User‑level systemd unit to run the llama.cpp server on localhost.  Depends on `linux‑zen‑headers`, `nvidia‑open‑dkms` and ensures the model is loaded before the daemon starts.",
          "interacts_with": [
            "third_party/llama.cpp/server",
            "models"
          ],
          "implementation": "Define a service with `ExecStart=/home/$USER/projects/VibeNote/third_party/llama.cpp/server -m /home/$USER/projects/VibeNote/models/<model>.gguf --host 127.0.0.1 --port 11434 -c 4096 -t $(nproc) -ngl 20 --no-webui`.  Use `ProtectSystem=strict`, `ProtectHome=yes`, `PrivateTmp=yes`, `NoNewPrivileges=yes`.  Set `Restart=always`.  Place the file under `~/.config/systemd/user/` during installation."
        },
        {
          "path": "packaging/systemd/vibed.service",
          "type": "file",
          "description": "User‑level systemd unit to start the VibeNote daemon API.  Depends on `vibemodel.service`.",
          "interacts_with": [
            "daemon/src/main.cpp"
          ],
          "implementation": "Define a service with `ExecStart=/home/$USER/projects/VibeNote/build/vibenote_daemon --config /home/$USER/.config/VibeNote/config.yml`.  Use similar hardening options as vibemodel.service.  Set `After=vibemodel.service`.  Place in `~/.config/systemd/user/`."
        }
      ]
    },
    {
      "path": "packaging/polkit/",
      "type": "directory",
      "description": "Polkit policy for screen capture permissions.",
      "children": [
        {
          "path": "packaging/polkit/org.saphyre.vibenote.policy",
          "type": "file",
          "description": "Defines polkit rules to allow the VibeNote daemon to access the screen capture portal without repeated user prompts.",
          "interacts_with": [],
          "implementation": "Write an XML policy file granting access to org.freedesktop.portal.ScreenCast for the VibeNote daemon, using org.saphyre.vibenote as the vendor ID.  Install this into the appropriate polkit directory via scripts/setup-arch.sh."
        }
      ]
    },
    {
      "path": "scripts/",
      "type": "directory",
      "description": "Automation scripts for setup, build, run, benchmarking and exporting.",
      "children": [
        {
          "path": "scripts/setup-arch.sh",
          "type": "file",
          "description": "Installs OS dependencies via pacman, configures DKMS, compiles llama.cpp with CUDA, copies systemd unit files and polkit policies, and downloads the model weights into the models directory.",
          "interacts_with": [
            "third_party/llama.cpp",
            "packaging/systemd/*",
            "packaging/polkit/org.saphyre.vibenote.policy",
            "models"
          ],
          "implementation": "Write a bash script that runs `sudo pacman -Syu` and installs `base-devel`, `cmake`, `ninja`, `qt6-base`, `qt6-declarative`, `kirigami6`, `nvidia-open-dkms`, `linux-zen-headers`, `pipewire`, `libportal`, `tesseract`, `sqlite`, `onnxruntime`, `nvml`, and other required packages.  Clone `third_party/llama.cpp` if not already present and run `make LLAMA_CUBLAS=1`.  Copy `vibemodel.service`, `vibed.service` to `~/.config/systemd/user/` and enable them.  Copy the polkit policy to `/etc/polkit-1/localauthority/10-vendor.d/`.  Download the 3B summariser model via `wget` or `aria2c` into `models/`.  Print next steps to the user.  Respect the Zen kernel guidelines (install linux-zen-headers before DKMS packages)."
        },
        {
          "path": "scripts/build.sh",
          "type": "file",
          "description": "Runs CMake configure, build and install with Ninja for both the app and daemon.  Accepts debug/release flags and optionally cleans the build directory.",
          "interacts_with": [
            "CMakeLists.txt"
          ],
          "implementation": "Implement a bash script that creates `build/` if it doesn’t exist, runs `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_PADDLE_OCR=ON/OFF` depending on environment variables, and then runs `cmake --build build --target install`.  Support a `--clean` flag that removes the build directory before reconfiguring."
        },
        {
          "path": "scripts/run-dev.sh",
          "type": "file",
          "description": "Launches the llama server and daemon in the foreground and runs the GUI for development.",
          "interacts_with": [
            "daemon/src/main.cpp",
            "third_party/llama.cpp"
          ],
          "implementation": "Start the llama.cpp server in a background process with the desired `-ngl` setting and model path.  Then launch the daemon binary (vibenote_daemon) on 127.0.0.1 with a development configuration.  Finally, start the GUI executable.  Use `trap` to kill both server and daemon on exit.  Print URLs for the API and hints for testing."
        },
        {
          "path": "scripts/bench.sh",
          "type": "file",
          "description": "Benchmark script to measure tokens per second, OCR throughput and end‑to‑end latency.  Outputs a summary to stdout and optional JSON.",
          "interacts_with": [
            "daemon/src/llama_client.cpp"
          ],
          "implementation": "Write a shell script that uses `curl` to send a series of summarisation requests via `/v1/summarize` and measures response times.  Use `nvidia-smi` or NVML via a small C++ helper to measure GPU utilisation during the test.  Save results to a CSV or JSON."
        },
        {
          "path": "scripts/export-examples.sh",
          "type": "file",
          "description": "Demonstrates usage of the export API by exporting notes in raw, JSON and structured prompts formats.  Useful for manual verification of export functionality.",
          "interacts_with": [
            "daemon/src/http_server.cpp"
          ],
          "implementation": "Use `curl` to call `/v1/export?format=raw`, `/v1/export?format=json`, and `/v1/export?format=structured_prompts` with a specified date range.  Save the responses to files in `./exports/`.  Print the location of the exported files."
        }
      ]
    },
    {
      "path": "tests/",
      "type": "directory",
      "description": "Unit tests and integration tests.  Organised by component: daemon, app and integration.  Uses CTest and mocks for external dependencies.",
      "children": [
        {
          "path": "tests/daemon/",
          "type": "directory",
          "description": "Tests for queue, GPU guard, OCR pipeline, API handlers and storage.  Use GoogleTest or Catch2.",
          "children": []
        },
        {
          "path": "tests/app/",
          "type": "directory",
          "description": "Tests for overlay controller, API client and metrics view using QtTest and QSignalSpy.",
          "children": []
        },
        {
          "path": "tests/integration/",
          "type": "directory",
          "description": "End‑to‑end tests covering watch mode, summarisation and export flows.  Use a test harness to spin up the llama server and daemon and interact via HTTP and the GUI.",
          "children": []
        }
      ]
    },
    {
      "path": ".clang-format",
      "type": "file",
      "description": "Clang‑format configuration enforcing project coding style.  Apply this with `clang-format -i` on all C++ and QML files during CI.",
      "interacts_with": [],
      "implementation": null
    },
    {
      "path": ".clang-tidy",
      "type": "file",
      "description": "Clang‑tidy configuration specifying static analysis checks and warnings.  Apply as part of the build.",
      "interacts_with": [],
      "implementation": null
    },
    {
      "path": "CMakePresets.json",
      "type": "file",
      "description": "CMake preset definitions for debug, release and packaging builds.  Use these to drive the build in scripts.",
      "interacts_with": [],
      "implementation": null
    },
    {
      "path": "CMakeLists.txt",
      "type": "file",
      "description": "Top‑level CMake file that adds subdirectories for app and daemon, defines global options and installs miscellaneous files.  Includes warnings.cmake and toolchain settings.",
      "interacts_with": [
        "app/CMakeLists.txt",
        "daemon/CMakeLists.txt"
      ],
      "implementation": "Call `cmake_minimum_required(VERSION 3.26)`, `project(VibeNote LANGUAGES CXX)`.  Include `cmake/toolchains.cmake` and `cmake/warnings.cmake`.  Add subdirectories for `app` and `daemon`.  Install top‑level files like `LICENSE` and `README.md`.  Define configuration options (e.g. `ENABLE_PADDLE_OCR`)."
    },
    {
      "path": "LICENSE",
      "type": "file",
      "description": "The project’s licence (GPL‑3.0 or later).  Include the full text and reference other licences as needed.",
      "interacts_with": [],
      "implementation": null
    },
    {
      "path": "README.md",
      "type": "file",
      "description": "Project readme.  Provides an overview, features, quick start instructions, system requirements, architecture diagrams, configuration, API reference and detailed usage examples.  It quotes the marketing blurb verbatim and emphasises local privacy.",
      "interacts_with": [
        "vibe_note_full_spec_implementation_plan_and_readme_for_codex.md",
        "daemon/openapi.yaml"
      ],
      "implementation": "Write a comprehensive README that explains what VibeNote does, highlights the local‑first design and GPU guard, lists prerequisites (Arch with Zen kernel, Wayland, NVML, NVidia open driver), describes installation via `scripts/setup-arch.sh`, shows how to use the GUI hotkey and dashboard, and references the OpenAPI specification.  Include sample requests and images of the GUI.  Document how to enable watch mode and how to enrich notes with external API keys."
    }
  ]
}
