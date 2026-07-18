# Proc Monitor — Monitor de Sistema Linux en TUI

**C++23 / FTXUI / /proc filesystem**

Monitor en tiempo real del sistema Linux que lee estadísticas directamente de `/proc/stat`, `/proc/meminfo`, `/proc/loadavg` y `/proc/uptime`. Incluye interfaz TUI interactiva con FTXUI y modo de salida JSON.

## Características

- **CPU en tiempo real**: Uso por núcleo con barras de progreso coloreadas (verde → amarillo → rojo)
- **Memoria del sistema**: Total, disponible, usada, swap — con barras y porcentajes
- **Información del sistema**: Load average (1/5/15 min), procesos activos/totales, uptime, número de núcleos
- **Dos modos de ejecución**: TUI interactiva (fullscreen) o `--json` para integración con scripts
- **Arquitectura RAII**: Manejo de errores con `std::expected` sin excepciones
- **Actualización cada segundo**: Hilo en background con `PostEvent` para re-renderizado
- **Cobertura de tests**: Unitarios + integración con Catch2

## Tecnologías

| Tecnología | Versión | Propósito |
|---|---|---|
| C++23 | GCC 14.2 | Estándar del lenguaje |
| CMake | 3.31+ | Build system con presets |
| FTXUI | 5.0 | Terminal UI framework |
| nlohmann_json | 3.11 | Serialización JSON |
| spdlog | 1.15 | Logging estructurado |
| Catch2 | 3.8 | Tests unitarios y de integración |

## Build

```bash
# Debug (con tests y sanitizers)
cmake --preset debug
cmake --build --preset debug

# Release (con LTO)
cmake --preset release
cmake --build --preset release
```

## Uso

```bash
# TUI interactiva
./build/debug/proc-monitor

# Salida JSON única (para scripts/paneles)
./build/debug/proc-monitor --json
```

### Salida JSON de ejemplo

```json
{
  "load_avg": {
    "1m": 0.45,
    "5m": 0.62,
    "15m": 0.58,
    "running": 2,
    "total_threads": 456
  },
  "uptime": {
    "seconds": 123456.78,
    "idle": 98765.43
  },
  "memory": {
    "total_kb": 16384000,
    "available_kb": 8192000,
    "free_kb": 4096000,
    "used_pct": 45.2,
    "available_pct": 50.0,
    "swap_total_kb": 2097152,
    "swap_free_kb": 1048576
  }
}
```

## Tests

```bash
cd build/debug && ctest --output-on-failure
# o directo:
./build/debug/tests/test_suite
```

## Arquitectura

```
src/
├── main.cpp              # Entry point, CLI parsing, JSON mode
├── proc_monitor.cpp       # FTXUI TUI dashboard (ScreenInteractive + Renderer)
├── cpu_stats.cpp          # /proc/stat reader + CPU delta calculator
└── system_info.cpp        # /proc/loadavg, uptime, meminfo readers

include/proc-monitor/
├── common.hpp             # Error / Result (std::expected)
├── cpu_stats.hpp          # CpuData, CpuUsage, compute_cpu_usage
├── system_info.hpp        # LoadAvg, UptimeInfo, MemInfo
└── proc_monitor.hpp       # MonitorState, run_tui()
```

### Pipeline de datos

```
/proc/stat ──→ read_proc_stat() ──→ compute_cpu_usage() ──→ TUI Renderer
/proc/meminfo ──→ read_meminfo() ──→ MemInfo ──→ barra + porcentaje
/proc/loadavg ──→ read_loadavg() ──→ LoadAvg ──→ texto
/proc/uptime ──→ read_uptime() ──→ UptimeInfo ──→ formato duración
```

## Licencia

MIT
