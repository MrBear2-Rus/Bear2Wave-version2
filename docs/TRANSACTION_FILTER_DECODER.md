# Transaction Filter Decoder Guide (Bear2Wave / GTKWave compatible subset)

Bear2Wave **L3 Transaction Filter Process** matches GTKWave's external-decoder model: GTKWave does **not** ship SPI/UART/CAN decoders in the viewer — it spawns **your** executable and passes a simplified VCD on **stdin**. GTKWave ships only a reference example: `examples/transaction.c` in the [gtkwave source](https://github.com/gtkwave/gtkwave).

Bear2Wave provides `tools/mock_transaction_proc.cmd` for the same purpose (CI + local demo), not a built-in protocol decoder.

## Workflow

1. User selects source signal(s) in the waveform pane.
2. Bear2Wave exports a **minimal VCD** for the chosen time window.
3. Bear2Wave writes that VCD to the decoder **stdin** (GTKWave uses the same pattern).
4. Decoder writes **stdout** lines; Bear2Wave parses them into virtual `[TXN]` rows + optional markers.

Configure path: **Edit → External Tool Paths → `transaction_proc`**, or env `BEAR2WAVE_TRANSACTION_PROC`.

## stdin (minimal VCD)

Bear2Wave exports:

```
$date / $version / $timescale
$scope … $var … $enddefinitions
#<t0>
b<value> <code>
#<t1>
…
```

GTKWave adds extra `$comment` headers (`min_time`, `max_time`, `seqn`, …). Decoders should tolerate their absence.

## stdout (supported subset)

| Line | Meaning |
|------|---------|
| `$name <label>` | Start a new virtual trace row |
| `$next` | Start another trace (auto-named `<prev>_next` if no `$name` follows) |
| `$finish` | End current trace block |
| `#<time> <value>` | Value change at `time` |
| `#<time> <color>?<text>` | GTKWave color prefix, e.g. `darkblue?sync` or `?gray24?04` |
| `M <time> <label>` | Named marker at `time` |

### Value types

- Scalar: `0`, `1`, `x`, `z`
- String / event: any other text (shown as string segments)

## Example (mock decoder output)

```
$name txn.data_nonzero
#400 1
#600 0
$name txn.last_data
#600 DEC:0000000000001111
M 600 txn_peak
```

## Session (.bwv v4)

Saved in `[transaction_traces]` + `txn=` display rows; restored without re-running the decoder.

## Reference links

- [GTKWave Filters documentation](https://gtkwave.github.io/gtkwave/quickstart/filters.html)
- Bear2Wave CLI test: `TraceTools test-fp2`
