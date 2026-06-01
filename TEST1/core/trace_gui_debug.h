#pragma once

/** GUI / FST lifecycle debug (enable with BEAR2WAVE_FST_DEBUG=1). */

/** True when env BEAR2WAVE_FST_DEBUG is non-empty and not "0". */
int trace_fst_debug_enabled(void);

/** Current OS thread id (for spotting concurrent fstReader use). */
unsigned long trace_fst_thread_id(void);

/**
 * Open/truncate FST debug log (call once at startup when debug is on).
 * Default file: <Bear2Wave.exe dir>/err.txt
 * Override: set env BEAR2WAVE_FST_DEBUG_FILE to a full path.
 */
void trace_fst_debug_init(void);

/** Path of the active FST debug log, or nullptr if debug off / not opened. */
const char* trace_fst_debug_log_path(void);

/**
 * Stage-tagged line to stderr + err.txt (+ bear2wave.log if BEAR2WAVE_LOG_FILE=1).
 * Example: trace_fst_log("LOAD_SYNC", "sig=%s rc=%d", name, rc);
 */
void trace_fst_log(const char* stage, const char* fmt, ...);
