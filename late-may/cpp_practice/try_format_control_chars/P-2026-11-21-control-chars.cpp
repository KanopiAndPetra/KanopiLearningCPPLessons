// P-2026-11-21 — try_format_control_chars:
//
// Closes Nov 20's "Where we go next" item (1), and finds a SECOND
// defect -- this time in the LINKED STDLIB, not in the arc's rule.
//
//   item (1) — "The control characters are the next collision battery.
//               GB3, GB4 and GB5 are unmodelled, and the four cells in
//               18.3 show the arc wrong in both directions. [...] Worth
//               building the battery that asks about them: one row per
//               rule, and one row per *ordering* of GB3/GB4/GB5 against
//               GB9."
//
// CLOSED -- one row per rule, and one row per ordering -- and the
// ordering is the lesson again.  UAX #29's rules are an ORDERED list,
// and GB3, GB4 and GB5 come BEFORE GB9, so an Extend after a NUL does
// NOT join the NUL's cluster: a control breaks on BOTH sides.
//
//   1. GB3   CR x LF                do not break between a CR and an LF
//   2. GB4   (Control | CR | LF) v     break AFTER a control
//   3. GB5   v (Control | CR | LF)     break BEFORE a control
//
// AND THE SECOND DEFECT IS IN THE ORACLE.  libc++ 21 has an ASCII FAST
// PATH: a string that BEGINS with ASCII code units is charged ONE
// COLUMN PER CODE UNIT for that prefix, and the cluster algorithm never
// runs over it.  The shortcut assumes an ASCII code point never joins
// another -- but GB3 says a CR and an LF DO.  So the linked stdlib
// charges a CR LF pair TWO columns at the start of an ASCII string and
// ONE column in the middle of a non-ASCII-leading one.  The ONE rule
// follows the LINKED STDLIB, so it models the fast path; the battery
// measures the pure rules against the shortcut, and the difference is
// printed rather than assumed.
//
// THE MEASUREMENT.  One battery of 19 control cells, FOUR models, one
// oracle: the width the LINKED stdlib charges.  A is Nov 20's rule, B
// consults the control rules AFTER GB9 (the wrong order), C is UAX
// #29's order, and D is today's ONE rule -- the order AND the fast
// path.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings, exactly as since
// Sep 6), the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables and its derived override table and
// its 1,596-cell grid, Nov 13's fill rule and its fill census, Nov 14's
// pair grid and the align/zero rule, Nov 15's triple grid and the
// recovery path, Nov 16's fill behaviour space, Nov 17's width census
// -- whose three numbers (1, 11, 28) are re-measured here and must not
// move -- Nov 18's precision battery (56 of 56), Nov 19's boundary
// battery (20 of 20), and Nov 20's collision battery (24 of 24), all
// of which are re-measured here and must not move.
//
// ============================================================================
// THE ARC BELOW — carried forward from Nov 20, with THREE changes: the
// CONTROL rules (GB3/GB4/GB5) are modelled and consulted BEFORE GB9,
// libc++ 21's ASCII FAST PATH is modelled, and Nov 20's four
// known-wrong control cells are CLOSED.  The header that follows is
// Nov 20's: it describes the RULE ORDER, which this file still
// implements.  Everything above is today's; everything below is the
// arc.
// ============================================================================

// P-2026-11-20 — try_format_rule_order:
//
// Closes Nov 19's "Where we go next" item (10), and finds TWO defects
// in the arc's own ONE column rule.
//
//   item (10) — "The GB9a/GB9c interaction was found by ACCIDENT, and
//               that is worth repeating deliberately. [...] Worth
//               searching for the other places where two cluster rules
//               can meet and pull in opposite directions: GB9a vs
//               GB9c is one; GB9 vs GB9c and GB11 vs GB9c are others."
//
// CLOSED, and the search found the other two.  UAX #29's rules are an
// ORDERED list -- GB9c is listed BEFORE GB11 -- and a rule CONSUMES
// its context:
//
//   1. THE ORDER.  When a conjunct's left context is live ("Consonant
//      [Extend Linker]* Linker [Extend Linker]*") and a ZWJ follows,
//      GB9c decides the boundary -- not GB11.  Nov 19's rule asked GB11
//      first, so "ka" virama ZWJ "ssa" was cut in two.
//
//   2. THE CONSUMPTION.  The conjunct spends its Linker: once it has
//      formed, a following Consonant needs a Linker of its own.  Nov
//      19's rule kept the Linker, so "ka" virama "ssa" "ssa" was ONE
//      cluster.
//
// THE MEASUREMENT.  One battery of 24 collision cells -- cells where
// TWO rules can meet -- FOUR models, one oracle: the width the LINKED
// stdlib charges.  A is Nov 19's rule, B spends the Linker, C asks
// GB9c first, D is today's ONE rule.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings, exactly as since
// Sep 6), the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables and its derived override table, its
// 1,596-cell grid, Nov 13's fill rule and its fill census, Nov 14's
// pair grid and the align/zero rule, Nov 15's triple grid and the
// recovery path, Nov 16's fill behaviour space, Nov 17's width census
// -- whose three numbers (1, 11, 28) are re-measured here and must not
// move -- Nov 18's precision battery (56 of 56), and Nov 19's
// boundary battery (20 of 20), all of which are re-measured here and
// must not move.
//
// ============================================================================
// THE ARC BELOW — carried forward from Nov 19, with TWO changes: the
// conjunct is consulted BEFORE GB11, and the conjunct CONSUMES its
// Linker.  The header that follows is Nov 19's: it describes the ONE
// COLUMN RULE, which this file still implements.  Everything above is
// today's; everything below is the arc.
// ============================================================================

// P-2026-11-19 — try_format_one_column_rule:
//
// Closes Nov 18's "Where we go next" item (1), and finds THREE MORE
// missing UAX #29 rules.
//
//   item (1) — "The GB11 fix belongs in the arc's OWN width rule, not
//               beside it.  Today the GB11 model lives in
//               `count_columns_gb11()` while the arc's Nov 17 rule still
//               lives in `count_field_width()` -- and the old rule is
//               now known to be wrong on two cells of today's battery.
//               The two should be ONE function, with the old rule
//               deleted, and the whole 28-cell width battery
//               re-measured afterwards.  Worth doing."
//
// CLOSED.  There is ONE column rule now: `count_columns()`.  Nov 17's
// rule -- the one that joined the code point after a ZWJ
// unconditionally -- is DELETED, and the 28-cell width battery
// (Section 15) and the 56-cell precision battery (Section 16) are
// re-measured with it; their numbers (1, 11, 28, and 56 of 56) do not
// move.
//
// AND THE MERGE PAID FOR ITSELF.  With both rules in ONE place, the
// battery can be widened to the CLUSTER BOUNDARIES the model does not
// implement -- and the model is found to be missing FIVE of UAX #29's
// rules: GB9a (x SpacingMark), GB9b (Prepend x), GB9c (the Indic
// conjunct), GB12/GB13 (the regional-indicator pairs) and GB6/GB7/GB8
// (the Hangul syllable sequences).  Five tables and five rules later,
// the ONE rule agrees with the linked stdlib on every cell of the
// boundary battery.
//
// THE MEASUREMENT.  One battery of 20 cells, THREE models, one oracle:
// the width the LINKED stdlib charges.  A is Nov 17's deleted rule, B is
// Nov 18's GB11 model, C is today's ONE rule.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings, exactly as since
// Sep 6), the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables and its derived override table, its
// 1,596-cell grid, Nov 13's fill rule and its fill census, Nov 14's
// pair grid and the align/zero rule, Nov 15's triple grid and the
// recovery path, Nov 16's fill behaviour space, and Nov 17's width
// census -- whose three numbers (1, 11, 28) are re-measured here and
// must not move -- all of which are re-measured here and must not move.
//
// ============================================================================
// THE ARC BELOW — carried forward from Nov 18, with ONE change: the two
// column rules are now ONE function, `count_columns()`, and Nov 17's
// rule is deleted.  The header that follows is Nov 18's: it describes
// the PRECISION, which this file still implements.  Everything above is
// today's; everything below is the arc.
// ============================================================================

// P-2026-11-18 — try_format_precision:
//
// Closes Nov 17's "Where we go next" item (3), and finds a DEFECT in
// the arc's own cluster rule.
//
//   item (3) — "The PRECISION has the same unit. [format.string.std]/15:
//               for string types the precision 'specifies the longest
//               prefix of the formatted argument to be included in the
//               replacement field such that the field width of the
//               prefix is no greater than the value of this option.'
//               So precision is measured in columns too — the same
//               table, the same unit. Worth measuring, and worth asking
//               whether the arc's scanner needs a second Unicode table
//               for it."
//
// THEY DO NOT WANT A SECOND TABLE.  The precision's unit is the COLUMN
// — the same field-width unit the width option uses — so the ONE
// table the arc already carries (Nov 17's kWidth2Ranges) decides BOTH
// rules.  What the precision needs that the width did not is the
// CLUSTER BOUNDARY: the prefix is cut at an extended grapheme cluster
// boundary, and a cluster is worth the width of its FIRST code point.
//
// THE MEASUREMENT.  A census of THREE candidate truncation rules over
// a battery of (argument, precision) cells, each scored against the
// LINKED stdlib's own output.  BYTES, CODE POINTS and COLUMNS are the
// three candidates, and only the column rule -- with the cut on a
// cluster boundary -- reproduces the stdlib on every cell.
//
// THE DEFECT.  UAX #29's GB11 joins the code point after a ZWJ only
// when the sequence is Extended_Pictographic Extend* ZWJ x
// Extended_Pictographic.  The arc's rule joined UNCONDITIONALLY.  It
// was right for the arc's battery — every ZWJ cell there is
// pictographic-to-pictographic — and wrong for "a" ZWJ U+1F600, which
// the stdlib charges THREE columns and the arc charged ONE.  Today's
// model implements GB11, and the arc's own width rule is measured
// against it.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings, exactly as since
// Sep 6), the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables and its derived override table, its
// 1,596-cell grid, Nov 13's fill rule and its fill census, Nov 14's
// pair grid and the align/zero rule, Nov 15's triple grid and the
// recovery path, Nov 16's fill behaviour space, and Nov 17's width
// census — whose three numbers (1, 11, 28) are re-measured here and
// must not move — all of which are re-measured here and must not move.
//
// ============================================================================
// THE ARC BELOW — carried forward from Nov 17, verbatim.  The header that
// follows is Nov 17's: it describes the FIELD WIDTH, which this file
// still implements.  Everything above is today's; everything below is
// the arc.
// ============================================================================
//
// P-2026-11-17 — try_format_field_width:
//
// Closes Nov 16's "Where we go next" items (2) and (5), and REFUTES one
// of the arc's own findings.
//
//   item (2) — "The chain item (3) — the width counts code points.
//               Still open, and now sharper: the width counts code
//               points of the *argument*, and the fill is repeated to
//               fill it."
//
//   THE PREMISE IS WRONG.  The width does not count code points.  It
//   counts FIELD-WIDTH UNITS -- the number of terminal columns -- and a
//   code point can be worth 1 column or 2.  U+1F600 is ONE code point
//   and TWO columns.
//
//   item (5) — "the argument is also a Unicode string, and the standard
//               counts the width of a string argument in code points
//               too.  Worth measuring whether the two rules want the
//               same table."
//
//   THEY DO NOT WANT THE SAME TABLE.  The fill is ONE scalar value, and
//   the fill's table is the Unicode scalar-value table.  The width is a
//   SUM over extended grapheme clusters, each worth the field width of
//   its FIRST code point, and the width's table is the field-width
//   table.  Two different tables, two different rules.
//
// THE DATUM.  A field-width model for the arc: the standard's rule
// ([format.string.std]/12), a RESTRICTED field-width table, and the
// cluster rule that decides which code points are free.
//
// THE MEASUREMENT.  A COLUMN CENSUS -- one battery of arguments
// measured three ways: bytes, code points, and the columns the LINKED
// stdlib actually charges -- plus the fill's own overflow.
//
// THE FINDINGS.
//
//   1. The width counts COLUMNS, not code points.  So Nov 13's "KEY
//      FINDING #4 -- THE WIDTH COUNTS CODE POINTS, NOT BYTES" is not
//      a rule: it is what the measurement said for arguments whose
//      every code point is one column wide.  The euro is one column
//      wide, and the euro was the whole battery.
//
//   2. The unit is the GRAPHEME CLUSTER, not the code point.  "e" plus
//      U+0301 is one column; the seven-code-point family ZWJ sequence
//      is two.
//
//   3. The FILL is assumed to be ONE column wide -- the standard says
//      so -- so a two-column fill pads the field WIDER than the width
//      asked for.  The fill's repetition count does not change; the
//      overflow happens anyway.
//
//   4. Both shipped implementations AGREE on every cell of the battery.
//      The standard's rule is a "should", but the two implementations
//      implement it the same way -- so this is a rule, not a policy.
//
//   5. The recovery's SUBSTITUTION COUNT is now measured, not assumed:
//      libstdc++ 15 substitutes ONE U+FFFD for the whole maximal
//      subpart -- the two-byte subpart "\xE2\x82" yields one U+FFFD
//      per repetition, not two.  And U+FFFD is ONE column wide, so the
//      recovered fill's width is one.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings, exactly as since
// Sep 6), the two-pass (measure, then write) design, the
// counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables and its derived override table, its
// 1,596-cell grid, Nov 13's fill rule and its fill census, Nov 14's
// pair grid and the align/zero rule, Nov 15's triple grid and the
// recovery path, and Nov 16's fill behaviour space -- all of which are
// re-measured here and must not move.
//
// ============================================================================
// THE ARC BELOW -- carried forward from Nov 16, verbatim.  The header that
// follows is Nov 16's: it describes the FILL BEHAVIOUR SPACE, which this
// file still implements.  Everything above is today's; everything below is
// the arc.
// ============================================================================
//
// P-2026-11-16 — try_format_fill_behaviour:
//
// Closes Nov 15's "Where we go next" item (1), verbatim:
//
//   item (1) — "The chain item (2) is now half-closed: pairs, not
//               triples.  The combination grid measures PAIRS of the
//               seven option slots.  The first cell that needs all
//               THREE of fill, align and '0' at once is the
//               interesting one — the fill's Unicode rule and the
//               align/zero rule have never met.  Worth measuring a
//               three-slot grid: 35 triples x 19 types x 7 rows = 4,655
//               cells, and asking whether the two rules are
//               independent."
//
// (That item was closed on Nov 15.  Today's item is the chain's own
// follow-up, quoted in the notes: "Worth asking whether the fill's
// two per-implementation facts want to be one datum: a single
// FillBehaviour struct holding the fatal mask and the recovery rule
// together, so the fill's whole enforcement is one value.")
//
// THE DATUM.  The fill's enforcement is ONE value -- FillBehaviour --
// and the scanner asks it ONE question: what do you do with THIS
// fault?  (FillOutcome: WellFormed, Fatal, Recoverable.)
//
// THE MEASUREMENT.  For the first time in the arc, the grid's axis is
// the POLICY, not the spec.  Every previous grid asked "does the
// model agree with the stdlib on every SPEC?"; today's asks "how many
// distinct BEHAVIOURS can the fill's enforcement space express?"
//
//   the space:      2 rules x 32 fault masks x 2 recovery rules = 128
//                   candidate values
//   the behaviours: 13 distinct signatures, not 128 -- 5 under the
//                   C++23 align/zero wording (4 scalar + 1 byte rule)
//                   and 8 under the C++20 one (7 scalar + 1 byte rule).
//
// THE FINDING.  The fatal mask has FIVE bits and THREE of them are
// DEAD.  Overlong, surrogate and above-U+10FFFF can never be
// recovered into a valid spec: their MAXIMAL SUBPART is the lead
// byte alone, so the byte the scanner looks at is still part of the
// ill-formed sequence and can never be the alignment option.  In this
// battery the mask's effective rank is TWO bits out of five -- and the
// two live ones are exactly the faults where recovery is reachable.
//
// And the recovery rule is COUPLED to the OTHER per-implementation
// fact: it is observable only under the C++20 wording of the align/
// zero rule, because that is the only wording in which the alignment
// option changes the '0' rule's answer.  Neither fact is measurable
// alone -- which is exactly why they want to be one datum.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings are exactly as they
// have been since Sep 6), the two-pass (measure, then write) design,
// the counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables and its derived override table, its
// 1,596-cell grid, Nov 13's fill rule and its fill census, Nov 14's
// pair grid and the align/zero rule, and Nov 15's triple grid and the
// recovery path -- all of which are re-measured here and must not move.
//
// ============================================================================
// THE ARC BELOW — carried forward from Nov 15, verbatim.  The header that
// follows is Nov 15's: it describes the TRIPLE GRID, which this file
// still implements.  Everything above is today's; everything below is
// the arc.
//
// ============================================================================
//
// P-2026-11-15 — try_format_triple_grid:
//
// Closes Nov 14's "Where we go next" item (1), verbatim:
//
//   item (1) — "The chain item (2) is now half-closed: pairs, not
//               triples.  The combination grid measures PAIRS of the
//               seven option slots.  The first cell that needs all
//               THREE of fill, align and '0' at once is the
//               interesting one — the fill's Unicode rule and the
//               align/zero rule have never met.  Worth measuring a
//               three-slot grid: 35 triples x 19 types x 7 rows = 4,655
//               cells, and asking whether the two rules are
//               independent."
//
// THE DEFECT.  The two rules meet in exactly ONE place: the cell
// where a fill, an align option and the '0' option are all present.
// And the arc's scanner loses the align option there.
//
//   The scanner RECORDS that an align option was present
//   (ParsedSpec::has_align), because the '0' rule needs to know it.
//   On the WELL-FORMED fill path it records it.  On the RECOVERY path
//   -- the path an implementation takes when it substitutes U+FFFD for
//   an ill-formed fill and carries on -- it CONSUMES the fill AND the
//   alignment option and records NEITHER.
//
//   So for a spec like "{:\x80<0}" the arc believes that no align
//   option was present, and applies the '0' rule as if there were
//   none.  The rule that was supposed to be INDEPENDENT of the fill's
//   rule is not.
//
// THE MEASUREMENT.  Neither shipped implementation can SEE this defect,
// which is why it survived Nov 13 and Nov 14:
//
//   libc++ 21     makes EVERY ill-formed fill fatal, so the recovery
//                 path never runs.
//   libstdc++ 15  runs the recovery path, but never lets the align
//                 option change the '0' (it always validity-checks
//                 it), so the lost align cannot change a verdict.
//
//   So the defect is invisible in both — and INVISIBLE IS NOT THE SAME
//   AS CORRECT.  Today's lesson builds the policy that CAN see it: an
//   implementation that RECOVERS from an ill-formed fill AND IGNORES
//   the '0' when an align option is present.  Under that policy the
//   arc's own rule composition is measurable, and it disagrees with
//   itself.
//
// THE GRID.  Nov 14 measured PAIRS (2,793 cells).  Today measures
// TRIPLES: C(7,3) = 35 triples x 19 type chars x 7 rows = 4,655 cells.
// A rule that only exists where THREE options meet is invisible to a
// pair grid, so the pair grid's silence was EVIDENCE, not PROOF.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings are exactly as they
// have been since Sep 6), the two-pass (measure, then write) design,
// the counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables and its derived override table, its
// 1,596-cell grid, Nov 13's fill rule and its fill census, and Nov
// 14's pair grid and the align/zero rule — all of which are
// re-measured here and must not move.
//
// ============================================================================
// THE ARC BELOW — carried forward from Nov 14, verbatim.  The header that
// follows is Nov 14's: it describes the COMBINATION GRID, which this file
// still implements.  Everything above is today's; everything below is the
// arc.
//
// ============================================================================
//
// P-2026-11-14 — try_format_combination_grid:
//
// Closes Nov 13's "Where we go next" item (2), verbatim:
//
//   item (2) — "Nov 12's item (2) — extend the grid past single
//               options.  Every one of the 1,596 cells isolates ONE
//               option, and not one of them reaches the fallback.
//               Worth measuring a combination grid — e.g. does
//               "{:+c}" on a char fail at phase 1 (sign not in the
//               mask) or phase 2 (narrowing)?"
//
// THE DEFECT.  A rule that only exists where TWO options MEET is
// invisible to a grid that puts ONE option in each cell.  Measured
// over the 21 PAIRS of the seven option slots (2,793 cells), the
// arc's model and libc++ 21 disagree on SEVEN cells — and all seven
// are one family: the align option next to the '0' option.
//
//   The pre-P2572R1 wording of [format.string.std]: "If the 0
//   character and an align option both appear, the 0 character is
//   ignored."  The CURRENT wording keeps the '0' option's VALIDITY
//   independent of the align option: "The 0 option is valid for
//   arithmetic types other than charT and bool or when an integer
//   presentation type is specified ... where n is 0 if the align
//   option is present".
//
// So the two implementations enforce DIFFERENT WORDINGS of the same
// rule, and the difference is a per-implementation datum:
//
//   libc++ 21     the '0' is IGNORED when an align option is present
//                  -> bool "{:<0}" is ACCEPTED
//   libstdc++ 15  the '0' is still VALIDITY-CHECKED
//                  -> bool "{:<0}" is REJECTED
//
// The fix is therefore not in the standard's rule but in the POLICY
// TABLE: a per-implementation boolean, exactly like Nov 13's fill
// rule and Nov 12's numeric ceiling.
//
// THE CHAIN ITEM'S OWN QUESTION, ANSWERED.  Does "{:+c}" on a char
// fail at phase 1 (the sign is not in the consume mask) or phase 2
// (the narrowing)?  It fails at PHASE 2: the sign IS consumed for
// charT, and it is the ALLOW mask that rejects it, because 'c' is
// not an integer presentation type for charT.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings are exactly as they
// have been since Sep 6), the two-pass (measure, then write) design,
// the counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables, its derived override table, and
// its 1,596-cell grid — which is re-measured here and must not move.
//
// ============================================================================
// THE ARC BELOW — carried forward from Nov 13, verbatim.  The header that
// follows is Nov 13's: it describes the FILL rule, which this file still
// implements.  Everything above is today's; everything below is the arc.
//
// ============================================================================
//
// P-2026-11-13 — try_format_unicode_fill:
//
// Closes Nov 12's "Where we go next" item (1), verbatim:
//
//   item (1) — "The Unicode fill rule — new today, and now measured.
//               The standard says the fill 'corresponds to a single
//               Unicode scalar value'; the arc's scanner is
//               BYTE-oriented: it reads the fill as one byte, so a
//               four-byte fill (U+1F921) is a malformed spec to the
//               arc and a valid one to libc++.  This lesson MEASURES
//               the false positive and leaves it visible; fixing it
//               needs a real UTF-8 decoder, which is the next link in
//               the chain."
//
// THE DEFECT.  [format.string.std]/3: "For a format specification in
// UTF-8, UTF-16, or UTF-32, the fill character corresponds to a
// single Unicode scalar value."  The fill is a CHARACTER, not a byte.
//
// The arc's scanner read ONE byte and asked "is the byte after it an
// alignment option?".  A four-byte fill therefore has no alignment
// byte after it, so the scanner fell through and called the spec
// malformed.  The stdlib accepts it.  That is a FALSE POSITIVE, and
// it was the last one the arc had.
//
// But the fix is not a one-line change, and the reason is the
// interesting part: the two implementations DISAGREE about what an
// ill-formed fill means, and they disagree in more than one way.
//
// KEY FINDING #1 — THE FILL IS A SCALAR VALUE, SO THE SCANNER NEEDS
// A UTF-8 DECODER, NOT A BIGGER BYTE WINDOW.
//
//   The scanner decodes one UTF-8 scalar value at the fill position,
//   using the rules of Unicode 15 Table 3-7 read BACKWARDS: the table
//   says which byte sequences are well-formed, and this says which
//   of the others it is -- truncated, invalid lead byte, overlong,
//   surrogate, out of range, bad continuation byte.
//
//   The decoder also reports the MAXIMAL SUBPART: how many bytes the
//   broken sequence occupies before the fault.  That number is what
//   an implementation that substitutes U+FFFD would replace, so it
//   is what the scanner has to skip.
//
// KEY FINDING #2 — THE TWO IMPLEMENTATIONS DISAGREE ABOUT ILL-FORMED
// FILLS, AND THE DIFFERENCE IS A MASK, NOT A BIT.
//
//   libc++ 21 rejects EVERY ill-formed fill: "The format specifier
//   contains malformed Unicode characters".
//
//   libstdc++ 15 makes only SOME of them fatal.  Overlong, surrogate
//   and above-U+10FFFF fills it rejects; the ones that merely stop
//   early -- truncated, or a lead byte that cannot start a sequence
//   -- it RECOVERS from, by substituting U+FFFD and carrying on.
//
//   So the policy table gains a MASK of the faults that are FATAL,
//   and the arc's scanner consults it.  This is the same shape as
//   Nov 7's per-implementation override rows, and the same shape as
//   Nov 12's numeric ceiling: the standard fixes the rule, each
//   implementation fixes its own enforcement of it.
//
// KEY FINDING #3 — THE ILL-FORMED FILL NO LONGER NEEDS THE MESSAGE
// TABLE.  IT IS A STRUCTURAL RULE NOW.
//
//   Nov 12's carried verdict rescued the invalid-UTF-8 fill under
//   libc++ ONLY because libc++'s message happened to be in the arc's
//   libc++-shaped table.  Today the scanner rejects the fill on its
//   own, so the verdict is FormatParse in BOTH builds -- and the
//   census's "moved by the carried verdict" count for the fill family
//   goes to ZERO, because the carry has nothing left to carry.
//
// KEY FINDING #4 — THE WIDTH COUNTS CODE POINTS, NOT BYTES.
//
//   A width of 10 with a four-byte fill produces THIRTY-ONE bytes:
//   ten code points, of which seven are the fill.  The arc's cap is
//   in BYTES (it is a buffer cap, and it is measured by the stdlib's
//   own output), so the arc is right and the two units must not be
//   confused: the WIDTH is a count of code points, the CAP is a count
//   of bytes.
//
// THE ANSWER TO THE CHAIN ITEM'S OWN QUESTION.
//
//   "decode the fill as a UTF-8 scalar value, validate it, and
//    re-measure the false-positive count".
//
//   All three, and both rules are measured against the LINKED stdlib
//   at once:
//
//     the byte-oriented rule   disagrees about the multi-byte fills
//                              (the arc rejects, the stdlib accepts)
//                              AND about the ill-formed ones (the arc
//                              accepts, the stdlib rejects)
//     the scalar-value rule     disagrees about NONE of them
//
//   The counts are in Section 9, and they are DIFFERENT under the two
//   toolchains -- because libc++ and libstdc++ disagree about which
//   ill-formed fills are fatal.  The same source is correct against
//   either, which is the point of the policy table.
//
// WHAT DID NOT CHANGE: `petra::FormatError` / `petra::FormatErrorKind`
// (the four kinds, their order and their meanings are exactly as they
// have been since Sep 6), the two-pass (measure, then write) design,
// the counting_output_iterator, the bounded_char_writer, the TooLarge
// contract, the buffer-atomicity behaviour, the type-category table
// from Oct 30, Nov 7's mask tables, its derived override table, and
// its 1,596-cell grid -- which is re-measured here and must not move.
//
//
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <expected>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// Part 1 — petra::FormatError / petra::FormatErrorKind
//           (verbatim from Sep 6 through Oct 30).
// ============================================================================

namespace petra {

enum class FormatErrorKind : unsigned char {
    FormatParse,
    FormatArgument,
    Format,
    TooLarge,
};

struct FormatError {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    std::size_t needed_bytes{0};

    FormatError() = default;
    FormatError(FormatErrorKind k, std::string m,
                std::size_t needed = 0) noexcept
        : kind(k), message(std::move(m)), needed_bytes(needed) {}

    [[nodiscard]] bool operator==(const FormatError& other) const noexcept {
        return kind == other.kind && message == other.message
            && needed_bytes == other.needed_bytes;
    }
    [[nodiscard]] bool operator!=(const FormatError& other) const noexcept {
        return !(*this == other);
    }
};

}  // namespace petra

// ============================================================================
// Part 2 — petra::counting_output_iterator
//           (verbatim from Oct 11 .. Oct 30).
// ============================================================================

namespace petra {

class counting_output_iterator {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        counting_output_iterator* it_;
        constexpr const proxy& operator=(char) const noexcept {
            ++it_->count_;
            if (it_->cb_) it_->cb_(it_->count_, it_->data_);
            return *this;
        }
    };

    constexpr counting_output_iterator(callback_type cb, void* data) noexcept
        : cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<counting_output_iterator*>(this)};
    }
    constexpr counting_output_iterator& operator++() noexcept { return *this; }
    constexpr counting_output_iterator operator++(int) noexcept { return *this; }

private:
    std::size_t count_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace petra

// ============================================================================
// Part 3 — petra::detail::bounded_char_writer
//           (verbatim from Oct 11 .. Oct 30).
// ============================================================================

namespace petra {
namespace detail {

class bounded_char_writer {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;
    using callback_type = void (*)(std::size_t, void*);

    struct proxy {
        bounded_char_writer* it_;
        constexpr const proxy& operator=(char c) const noexcept {
            if (it_->written_ < it_->cap_) {
                it_->buf_[it_->written_] = c;
                ++it_->written_;
                if (it_->cb_) it_->cb_(it_->written_, it_->data_);
            }
            return *this;
        }
    };

    constexpr bounded_char_writer(char* buf, std::size_t cap,
                                  callback_type cb, void* data) noexcept
        : buf_(buf), cap_(cap), cb_(cb), data_(data) {}

    constexpr proxy operator*() const noexcept {
        return proxy{const_cast<bounded_char_writer*>(this)};
    }
    constexpr bounded_char_writer& operator++() noexcept { return *this; }
    constexpr bounded_char_writer operator++(int) noexcept { return *this; }

    std::size_t written() const noexcept { return written_; }

private:
    char* buf_;
    std::size_t cap_;
    std::size_t written_{0};
    callback_type cb_;
    void* data_;
};

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 4 — petra::detail::ArgCategory and the type-category table.
//           (verbatim from Oct 30.)
// ============================================================================

namespace petra {
namespace detail {

enum class ArgCategory : unsigned char {
    Boolean,
    Character,       // charT
    Integral,        // integral types other than bool and charT
    FloatingPoint,
    String,
    Pointer,
    UserDefined,     // a formatter specialization: rules unknown
};

[[nodiscard]] constexpr std::string_view category_name(ArgCategory c) noexcept {
    switch (c) {
    case ArgCategory::Boolean:      return "bool";
    case ArgCategory::Character:    return "charT";
    case ArgCategory::Integral:     return "integral";
    case ArgCategory::FloatingPoint:return "floating-point";
    case ArgCategory::String:       return "string";
    case ArgCategory::Pointer:      return "pointer";
    case ArgCategory::UserDefined:  return "user-defined";
    }
    return "?";
}

template <class T>
[[nodiscard]] constexpr ArgCategory category_of() noexcept {
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, bool>) {
        return ArgCategory::Boolean;
    } else if constexpr (std::is_same_v<U, char>) {
        return ArgCategory::Character;
    } else if constexpr (std::is_same_v<U, wchar_t> || std::is_same_v<U, char8_t>
                         || std::is_same_v<U, char16_t> || std::is_same_v<U, char32_t>) {
        return ArgCategory::UserDefined;
    } else if constexpr (std::is_integral_v<U>) {
        return ArgCategory::Integral;
    } else if constexpr (std::is_floating_point_v<U>) {
        return ArgCategory::FloatingPoint;
    } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>
                         || std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
        return ArgCategory::String;
    } else if constexpr (std::is_array_v<U>
                         && std::is_same_v<std::remove_extent_t<U>, char>) {
        return ArgCategory::String;
    } else if constexpr (std::is_same_v<U, void*> || std::is_same_v<U, const void*>
                         || std::is_same_v<U, std::nullptr_t>) {
        return ArgCategory::Pointer;
    } else {
        return ArgCategory::UserDefined;
    }
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 5 — NEW TODAY: THE POLICY TABLE.
//
// This is the whole point of the lesson, so it is worth being precise
// about what "a table" means here.
//
// Oct 30's model had FIVE mask functions:
//
//   phase1_mask(cat)               libc++'s PARSE (consume) mask
//   standard_base_mask(cat)       the standard's base permissions
//   standard_effective_mask(c,t)  the standard's mask for (cat, type)
//   phase2_mask(cat, type)        libc++'s mask for (cat, type)
//   effective_mask(cat, type, p)  the if/else between the two models
//
// The table below replaces all five with:
//
//   TWO shared data tables      the standard's BASE permissions per
//                               category, and the integer-presentation
//                               types per category;
//   ONE per-implementation      a list of ROWS, each row an override
//   ROW list                    of one (category, type) cell.
//
// The split of the old model into "base" + "integer-presentation-type
// widening" is what makes the override table expressible at all: the
// STANDARD's rules fall out of the base table plus the widening rule,
// and an implementation's difference is a cell in the resulting
// matrix.
//
// The rows are keyed by (category, type), not by (category, boolean).
// That is the difference between "one boolean between two code paths"
// and "one row in one table", and it is what lets a third
// implementation be added without touching a branch.
// ============================================================================

namespace petra {
namespace detail {

enum class Opt : unsigned char {
    None       = 0u,
    Sign       = 1u << 0,
    Alternate  = 1u << 1,
    ZeroPad    = 1u << 2,
    Precision  = 1u << 3,
    Locale     = 1u << 4,
};

// The mask is a SCOPED ENUM, not a bare unsigned char, for one
// reason: an operator cannot be overloaded for a built-in type.  The
// enumerators carry the same bit values as Opt's, so a mask and an
// option remain interchangeable bit for bit — and `mask_of` is the one
// place that conversion happens.
enum class OptMask : unsigned char {
    None      = 0u,
    Sign      = 1u << 0,
    Alternate = 1u << 1,
    ZeroPad   = 1u << 2,
    Precision = 1u << 3,
    Locale    = 1u << 4,
};

[[nodiscard]] constexpr OptMask mask_of(Opt o) noexcept {
    return static_cast<OptMask>(static_cast<unsigned char>(o));
}

[[nodiscard]] constexpr OptMask operator|(OptMask a, OptMask b) noexcept {
    return static_cast<OptMask>(static_cast<unsigned char>(a)
                                 | static_cast<unsigned char>(b));
}

[[nodiscard]] constexpr OptMask operator&(OptMask a, OptMask b) noexcept {
    return static_cast<OptMask>(static_cast<unsigned char>(a)
                                 & static_cast<unsigned char>(b));
}

[[nodiscard]] constexpr bool has(OptMask m, Opt o) noexcept {
    return (m & mask_of(o)) != OptMask::None;
}

// A mask, rendered as the options it contains.  Used by Section 2 to
// print the EXACT difference between two policies — printing only the
// sign bit (as the draft did) under-reports a three-bit difference.
[[nodiscard]] std::string mask_text(OptMask m) {
    std::string s;
    const auto add = [&s, m](Opt o, const char* name) {
        if (!has(m, o)) return;
        if (!s.empty()) s += ' ';
        s += name;
    };
    add(Opt::Sign, "sign");
    add(Opt::Alternate, "alt");
    add(Opt::ZeroPad, "zero");
    add(Opt::Precision, "prec");
    add(Opt::Locale, "locale");
    if (s.empty()) s = "(none)";
    return s;
}

constexpr OptMask kMNone     = mask_of(Opt::None);
constexpr OptMask kMSign     = mask_of(Opt::Sign);
constexpr OptMask kMAlternate = mask_of(Opt::Alternate);
constexpr OptMask kMZero     = mask_of(Opt::ZeroPad);
constexpr OptMask kMPrecision = mask_of(Opt::Precision);
constexpr OptMask kMLocale   = mask_of(Opt::Locale);

// The options the standard's "or when an integer presentation type is
// specified" clause ADDS to a category's base permissions.
constexpr OptMask kIntegerTypeWidening = kMSign | kMAlternate | kMZero;

// ---- TABLE 1: the standard's BASE permissions, per category. ---------------
//
//   bool / charT      locale only: the sign, alternate and zero options
//                     are valid for them only when an integer
//                     presentation type is specified, and that is the
//                     widening rule's job, not the base's.
//   integral          sign alt zero locale
//   floating-point    sign alt zero prec locale
//   string            prec
//   pointer           zero
//   user-defined      nothing: the validator abstains for it.
struct BaseRow {
    ArgCategory category;
    OptMask base;
};

constexpr BaseRow kBaseTable[] = {
    {ArgCategory::Boolean,       kMLocale},
    {ArgCategory::Character,     kMLocale},
    {ArgCategory::Integral,      kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::FloatingPoint, kMSign | kMAlternate | kMZero | kMPrecision | kMLocale},
    {ArgCategory::String,        kMPrecision},
    {ArgCategory::Pointer,       kMZero},
    {ArgCategory::UserDefined,   kMNone},
};

[[nodiscard]] constexpr OptMask base_mask(ArgCategory c) noexcept {
    for (const BaseRow& row : kBaseTable) {
        if (row.category == c) return row.base;
    }
    return kMNone;
}

// ---- TABLE 2: the integer-presentation types, per category. ---------------
//
// The standard's Tables 107/108/109.  Note that 'c' is an integer
// presentation type for INTEGRAL but NOT for charT: charT's own 'c'
// means "the character", which is a different thing.
//
// A selected type in this set is what makes the sign / alternate /
// zero options valid for bool and charT.
struct IntPresRow {
    ArgCategory category;
    const char* types;
};

constexpr IntPresRow kIntPresTable[] = {
    {ArgCategory::Boolean,   "bBdoxX"},
    {ArgCategory::Character, "bBdoxX"},
    {ArgCategory::Integral,  "bBcdoxX"},
};

[[nodiscard]] constexpr bool is_integer_presentation_type(ArgCategory c, char t) noexcept {
    if (t == '\0') return false;
    for (const IntPresRow& row : kIntPresTable) {
        if (row.category != c) continue;
        for (const char* p = row.types; *p != '\0'; ++p) {
            if (*p == t) return true;
        }
    }
    return false;
}

// ---- TABLE 3: the CONSUME mask, per category. -----------------------------
//
// The options the PARSER will consume for this category.  An option
// outside the consume mask is not parsed at all: it is left in the
// input, and the leftover text is what trips libc++'s "should consume
// the input or end with a '}'" check.
//
// This is libc++'s `__fields_*` set, and the measurement shows
// libstdc++ 15 consumes the SAME set (see Part 13, Section 5).
struct ConsumeRow {
    ArgCategory category;
    OptMask mask;
};

constexpr ConsumeRow kSharedConsumeTable[] = {
    {ArgCategory::Boolean,       kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::Character,     kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::Integral,      kMSign | kMAlternate | kMZero | kMLocale},
    {ArgCategory::FloatingPoint, kMSign | kMAlternate | kMZero | kMPrecision | kMLocale},
    {ArgCategory::String,        kMPrecision},
    {ArgCategory::Pointer,       kMZero},
};

// ---- TABLE 4: the ALLOW mask, per category. -------------------------------
//
// The options the argument actually ACCEPTS, before any
// implementation-specific narrowing.  This is the standard's base mask
// widened by the integer-presentation-type clause.
[[nodiscard]] constexpr OptMask allow_mask(ArgCategory c, char type) noexcept {
    OptMask m = base_mask(c);
    if (is_integer_presentation_type(c, type)) {
        m = m | kIntegerTypeWidening;
    }
    return m;
}

// ---- TABLE 5: the per-implementation OVERRIDE rows. -----------------------
//
// One row overrides one (category, type) cell of the allow matrix.
//
// `any_type` rows apply to every type char of the category; otherwise
// the row applies only to `type_char`.
struct AllowRow {
    ArgCategory category;
    char type_char;
    bool any_type;
    OptMask mask;
};

// libc++ 21 narrows the integral mask to {locale} whenever the
// selected presentation type is 'c'.  The standard does not, because 'c'
// IS an integer presentation type for integral (Table 107), so the
// sign / alternate / zero options are valid there.
//
// This is the upstream bug: llvm/llvm-project#106136 and
// microsoft/STL#4918.
constexpr AllowRow kLibcxx21AllowRows[] = {
    {ArgCategory::Integral, 'c', false, kMLocale},
};

// ---- the fill's Unicode rule (NEW TODAY). -------------------------------
//
// [format.string.std]/3: "For a format specification in UTF-8, UTF-16,
// or UTF-32, the fill character corresponds to a single Unicode scalar
// value."  So the fill is a CHARACTER, not a byte.
//
// The two implementations differ about what an ill-formed fill means,
// and they differ in MORE THAN ONE WAY: libc++ 21 rejects every
// ill-formed fill, while libstdc++ 15 rejects the ones that decode to
// a value that is not a scalar value (overlong, surrogate, above
// U+10FFFF) and the ones with a bad continuation byte, and TOLERATES
// the ones that merely do not decode (truncated, invalid lead byte) by
// substituting U+FFFD.
//
// So the per-implementation datum is a MASK of the faults that are
// rejected -- the same shape as Nov 7's override rows and Nov 12's
// numeric ceiling: the standard fixes the rule, each implementation
// fixes its own enforcement of it.
enum class FillFault : unsigned char {
    None,
    // The sequence stops early: either the input ran out, or a byte
    // that cannot continue it came next.  What was consumed is the
    // MAXIMAL SUBPART (Unicode D93b): the longest prefix that could
    // still be completed into a well-formed sequence.
    Truncated,
    // The lead byte cannot start any well-formed sequence at all:
    // a continuation byte (0x80..0xBF) or 0xF8..0xFF.
    InvalidLead,
    // A complete sequence that names a value below the minimum for
    // its length: a non-minimal encoding of a smaller value.
    Overlong,
    // A complete sequence that names U+D800..U+DFFF.
    Surrogate,
    // A complete sequence that names a value above U+10FFFF.
    OutOfRange,
};

constexpr std::uint8_t kFaultTruncated   = 1u << 0;
constexpr std::uint8_t kFaultInvalidLead = 1u << 1;
constexpr std::uint8_t kFaultOverlong    = 1u << 2;
constexpr std::uint8_t kFaultSurrogate   = 1u << 3;
constexpr std::uint8_t kFaultOutOfRange  = 1u << 4;

constexpr std::uint8_t kFaultAll =
    kFaultTruncated | kFaultInvalidLead | kFaultOverlong | kFaultSurrogate
    | kFaultOutOfRange;

[[nodiscard]] constexpr std::uint8_t fault_bit(FillFault f) noexcept {
    switch (f) {
    case FillFault::None:             return 0;
    case FillFault::Truncated:        return kFaultTruncated;
    case FillFault::InvalidLead:      return kFaultInvalidLead;
    case FillFault::Overlong:         return kFaultOverlong;
    case FillFault::Surrogate:        return kFaultSurrogate;
    case FillFault::OutOfRange:       return kFaultOutOfRange;
    }
    return 0;
}

[[nodiscard]] constexpr const char* fault_name(FillFault f) noexcept {
    switch (f) {
    case FillFault::None:             return "None";
    case FillFault::Truncated:        return "Truncated";
    case FillFault::InvalidLead:      return "InvalidLead";
    case FillFault::Overlong:         return "Overlong";
    case FillFault::Surrogate:        return "Surrogate";
    case FillFault::OutOfRange:       return "OutOfRange";
    }
    return "?";
}

// What the implementation does with an ILL-FORMED fill.
//
// NEW TODAY: the fill's enforcement is ONE datum, and the scanner asks
// it ONE question -- "what do you do with THIS fault?" -- instead of
// reading three fields and deciding for itself.
enum class FillOutcome : unsigned char {
    WellFormed,   // the bytes decode to a Unicode scalar value
    Fatal,        // ill-formed, and this implementation rejects it
    Recoverable,  // ill-formed, and this implementation recovers from it
};

// The fill's WHOLE enforcement, as ONE value.
//
// Nov 15's next step asked for this exactly:
//
//   "Worth asking whether the fill's two per-implementation facts want
//    to be one datum: a single FillBehaviour struct holding the fatal
//    mask and the recovery rule together, so the fill's whole
//    enforcement is one value."
//
// They do.  And Section 14 MEASURES what the one value is worth: its
// space is 256 candidate values, but it can only tell 13 behaviours
// apart.  A datum's size is not the number of its fields; it is the
// number of behaviours it can distinguish.
struct FillBehaviour {
    // Whether the fill is decoded as a Unicode scalar value at all.
    // The OLD rule read one byte; the NEW rule reads a scalar value.
    //
    // This is the RULE, not an enforcement: it selects which grammar
    // the scanner applies.  Section 14 shows that under the byte rule
    // the whole enforcement is INERT -- there is no fault to be fatal
    // about and no recovery to keep an align through.
    bool unicode{true};

    // Which ill-formed fills this implementation treats as FATAL.  The
    // rest it RECOVERS from, by substituting U+FFFD for the maximal
    // subpart and carrying on.
    std::uint8_t fatal{0};

    // When this implementation RECOVERS from an ill-formed fill, does
    // the alignment option that follows it SURVIVE the recovery?
    //
    // Recovery means: substitute U+FFFD for the maximal subpart, and
    // carry on.  The substituted character is the fill, and the
    // character after it is still the alignment option.  So the answer
    // is YES for an implementation that recovers -- and the scanner
    // must RECORD it, because the '0' rule needs to know whether an
    // align option was present.
    //
    // The arc used to say NO here: on the recovery path it consumed
    // the fill AND the alignment option and recorded NEITHER.  That is
    // invisible in libc++ 21 (which never recovers) and invisible in
    // libstdc++ 15 (which always validity-checks the '0'), so the
    // defect survived two lessons.  See Section 13.
    bool recovery_keeps_align{true};

    [[nodiscard]] constexpr bool is_fatal(FillFault f) const noexcept {
        return (fatal & fault_bit(f)) != 0;
    }

    // NEW TODAY: the ONE question the scanner asks of the fill.
    //
    // The rule and the enforcement are read together here, so the
    // scanner never has to know how the two interact.
    [[nodiscard]] constexpr FillOutcome outcome(FillFault f) const noexcept {
        if (f == FillFault::None) return FillOutcome::WellFormed;
        return is_fatal(f) ? FillOutcome::Fatal : FillOutcome::Recoverable;
    }
};

// One decoded scalar value, and what was wrong with it.
struct Utf8CodePoint {
    char32_t value{0};
    std::size_t length{0};    // bytes consumed (the maximal subpart)
    FillFault fault{FillFault::None};
};

// Decode ONE UTF-8 scalar value at the front of `s`.
//
// The rules are Unicode 15 Table 3-7 (Well-Formed UTF-8 Byte
// Sequences), read from the other end: the table says which byte
// sequences are WELL-FORMED, and this says which of the others it is.
//
// `length` is the number of bytes the MAXIMAL SUBPART occupies: the
// bytes consumed before the fault.  That is what an implementation
// that substitutes U+FFFD would replace, so it is what the scanner
// has to skip.
[[nodiscard]] constexpr Utf8CodePoint decode_utf8(std::string_view s) noexcept {
    Utf8CodePoint out{};
    if (s.empty()) return out;

    const auto b0 = static_cast<unsigned char>(s[0]);
    if (b0 < 0x80u) {
        out.value = static_cast<char32_t>(b0);
        out.length = 1;
        return out;
    }

    // The shape of the sequence the lead byte announces, and the range
    // the SECOND byte has to be in.  The bounds are the ones in
    // Unicode 15 Table 3-7: E0's second byte must be A0..BF (else the
    // encoding is overlong), ED's must be 80..9F (else it names a
    // surrogate), F0's must be 90..BF (overlong), F4's must be
    // 80..8F (above U+10FFFF).
    std::size_t want = 0;
    char32_t value = 0;
    unsigned char lo2 = 0x80u;
    unsigned char hi2 = 0xBFu;
    FillFault bound = FillFault::None;

    if (b0 >= 0xC0u && b0 <= 0xDFu) {
        // 0xC0 and 0xC1 are OVERLONG-ONLY leads: they can only ever
        // encode a value below U+0080, and the completion check below
        // is what catches them.
        want = 2;
        value = static_cast<char32_t>(b0 & 0x1Fu);
    } else if (b0 >= 0xE0u && b0 <= 0xEFu) {
        want = 3;
        value = static_cast<char32_t>(b0 & 0x0Fu);
        if (b0 == 0xE0u) {
            lo2 = 0xA0u;
            bound = FillFault::Overlong;
        } else if (b0 == 0xEDu) {
            hi2 = 0x9Fu;
            bound = FillFault::Surrogate;
        }
    } else if (b0 >= 0xF0u && b0 <= 0xF7u) {
        want = 4;
        value = static_cast<char32_t>(b0 & 0x07u);
        if (b0 == 0xF0u) {
            lo2 = 0x90u;
            bound = FillFault::Overlong;
        } else if (b0 == 0xF4u) {
            hi2 = 0x8Fu;
            bound = FillFault::OutOfRange;
        }
    } else {
        // A continuation byte, or 0xF8..0xFF: never a lead byte.
        out.fault = FillFault::InvalidLead;
        out.length = 1;
        return out;
    }

    std::size_t k = 1;
    for (; k < want && k < s.size(); ++k) {
        const auto b = static_cast<unsigned char>(s[k]);
        if (b < 0x80u || b > 0xBFu) {
            // A byte that cannot continue the sequence: the sequence
            // stops here, and what came before is the maximal subpart.
            break;
        }
        if (k == 1 && bound != FillFault::None && (b < lo2 || b > hi2)) {
            // The second byte is outside the lead's range, so the
            // sequence is overlong, a surrogate, or out of range.  The
            // maximal subpart is the LEAD BYTE ALONE: "E0 80" is not a
            // prefix of any well-formed sequence.
            out.fault = bound;
            out.length = 1;
            return out;
        }
        value = static_cast<char32_t>((value << 6)
                                       | static_cast<char32_t>(b & 0x3Fu));
    }

    if (k < want) {
        // The sequence stops early.  What was consumed is a valid
        // prefix, so it IS the maximal subpart.
        out.fault = FillFault::Truncated;
        out.length = k;
        return out;
    }

    // The sequence is complete: check that it names a scalar value.
    const char32_t min_value =
        want == 2 ? 0x80u : (want == 3 ? 0x800u : 0x10000u);
    if (value < min_value) {
        out.fault = FillFault::Overlong;
        out.length = 1;
        return out;
    }
    if (value >= 0xD800u && value <= 0xDFFFu) {
        out.fault = FillFault::Surrogate;
        out.length = 1;
        return out;
    }
    if (value > 0x10FFFFu) {
        out.fault = FillFault::OutOfRange;
        out.length = 1;
        return out;
    }

    out.value = value;
    out.length = want;
    return out;
}

// Count the scalar values in a UTF-8 string.  Used by Section 9 to
// check what the stdlib counts when it counts a WIDTH.
[[nodiscard]] constexpr std::size_t count_code_points(std::string_view s) noexcept {
    std::size_t n = 0;
    std::size_t i = 0;
    while (i < s.size()) {
        const Utf8CodePoint cp = decode_utf8(s.substr(i));
        if (cp.length == 0) break;
        i += cp.length;
        ++n;
    }
    return n;
}

// ---- The implementations the arc knows about. ------------------------------
//
// "standard"      the portable rules of [format.string.std]
// "libc++ 21"     libc++ 21's rules, bug included
// "libstdc++ 15"  libstdc++ 15's rules (measured: the standard's)
struct ImplPolicy {
    const char* name;
    const AllowRow* allow_rows;
    std::size_t allow_count;
    const ConsumeRow* consume_rows;
    std::size_t consume_count;

    // The largest numeric literal this implementation can represent --
    // the width, the precision, or an argument index.  std::nullopt means
    // "these rules fix no ceiling": the standard's grammar admits any
    // positive-integer, and only the implementation's parser objects.
    //
    // Note the SHAPE: Nov 7's per-implementation difference was a mask
    // CELL, keyed by (category, type).  This one is a SCALAR.  The
    // override-row machinery still applies to the masks; this is a second
    // kind of policy datum, and it needs a second kind of lookup.
    std::optional<std::uint64_t> max_number{std::nullopt};

    // Where the ceiling comes from, in the implementation's own words.
    const char* ceiling_note{""};

    // NEW TODAY: how this implementation reads the fill.
    //
    // The standard fixes the RULE -- the fill is a single Unicode
    // scalar value -- but not the ENFORCEMENT, and the two
    // implementations enforce it differently.  So the fill's rule is
    // the standard's and lives in the scanner; what lives here is
    // which ill-formed fills this implementation rejects.
    FillBehaviour fill{};

    // NEW TODAY: does an ALIGN option make the '0' option IGNORED?
    //
    // [format.string.std] has TWO WORDINGS of the align/zero rule,
    // and they disagree about VALIDITY, not about padding:
    //
    //   C++20   "If the 0 character and an align option both appear,
    //            the 0 character is ignored."
    //            -> the '0' is not validity-checked AT ALL.
    //
    //   C++23   the 0 option "is valid for arithmetic types other
    //            than charT and bool or when an integer presentation
    //            type is specified", and the padding count is "n is 0
    //            if the align option is present".
    //            -> the '0' IS validity-checked; align only zeroes
    //            the padding.
    //
    // MEASURED (Section 11): libc++ 21 implements the C++20 wording
    // -- a bool ACCEPTS "{:<0}" -- and libstdc++ 15 implements the
    // C++23 one -- a bool REJECTS "{:<0}".  The standard's own
    // wording is the C++23 one.
    //
    // So this is a per-implementation datum, exactly like Nov 12's
    // numeric ceiling and Nov 13's fill mask: the standard fixes the
    // rule, each implementation fixes its own enforcement of it.
    bool zero_ignored_with_align{false};
};

constexpr ImplPolicy kImpls[] = {
    {"standard",     nullptr,                  0, kSharedConsumeTable, 6,
     std::nullopt,
     "the grammar admits any positive-integer: no ceiling",
     // The standard: the fill is a single scalar value, so every
     // ill-formed fill is a malformed format string.
     FillBehaviour{true, kFaultAll},
     // The standard's CURRENT wording of the align/zero rule: the '0'
     // option is valid-or-not regardless of the align option.
     false},
    {"libc++ 21",    kLibcxx21AllowRows,       1, kSharedConsumeTable, 6,
     std::uint64_t{2147483647},
     "libc++ 21: __format/format_string.h, __number_max = INT32_MAX",
     // MEASURED: libc++ 21 rejects every ill-formed fill with
     // "The format specifier contains malformed Unicode characters".
     FillBehaviour{true, kFaultAll},
     // MEASURED: libc++ 21 implements the C++20 wording of the
     // align/zero rule -- "if the 0 character and an align option both
     // appear, the 0 character is ignored" -- so a bool ACCEPTS
     // "{:<0}".  See __parse_zero_padding() in
     // __format/parser_std_format_spec.h: the '0' only becomes the
     // zero-padding alignment when no align option was given.
     true},
    {"libstdc++ 15", nullptr,                  0, kSharedConsumeTable, 6,
     std::uint64_t{65535},
     "libstdc++ 15: format, __parse_integer() returns unsigned short",
     // MEASURED: libstdc++ 15 makes overlong, surrogate and
     // above-U+10FFFF fills FATAL, and RECOVERS from the truncated and
     // invalid-lead ones by substituting U+FFFD.
     FillBehaviour{true, kFaultOverlong | kFaultSurrogate | kFaultOutOfRange},
     // MEASURED: libstdc++ 15 implements the CURRENT wording: a bool
     // REJECTS "{:<0}", so the '0' is still validity-checked.
     false},
};

constexpr std::size_t kImplCount = sizeof(kImpls) / sizeof(kImpls[0]);

[[nodiscard]] constexpr std::size_t impl_index_of(const char* name) noexcept {
    for (std::size_t i = 0; i < kImplCount; ++i) {
        std::size_t j = 0;
        for (; kImpls[i].name[j] != '\0' && name[j] != '\0'; ++j) {
            if (kImpls[i].name[j] != name[j]) break;
        }
        if (kImpls[i].name[j] == '\0' && name[j] == '\0') return i;
    }
    return kImplCount;
}

// The effective ALLOW mask for (category, type) under one policy: the
// standard's matrix, then the policy's override rows.
[[nodiscard]] constexpr OptMask
policy_allow_mask(ArgCategory c, char type, const ImplPolicy& impl) noexcept {
    OptMask m = allow_mask(c, type);
    for (std::size_t i = 0; i < impl.allow_count; ++i) {
        const AllowRow& row = impl.allow_rows[i];
        if (row.category != c) continue;
        if (!row.any_type && row.type_char != type) continue;
        m = row.mask;
    }
    return m;
}

// The CONSUME mask for a category under one policy.
[[nodiscard]] constexpr OptMask
policy_consume_mask(ArgCategory c, const ImplPolicy& impl) noexcept {
    OptMask m = kMNone;
    for (std::size_t i = 0; i < impl.consume_count; ++i) {
        if (impl.consume_rows[i].category == c) m = impl.consume_rows[i].mask;
    }
    return m;
}

// The implementation the arc should model: the one it is LINKED
// against.  This is what makes the same source correct under both
// toolchains with no source change at all.
[[nodiscard]] constexpr const ImplPolicy& linked_policy() noexcept {
#if defined(_LIBCPP_VERSION)
    return kImpls[impl_index_of("libc++ 21")];
#elif defined(_GLIBCXX_RELEASE)
    // NOTE: the macro is _GLIBCXX_RELEASE — ONE leading underscore.
    // The double-underscore spelling is not defined by any compiler,
    // so the draft's `#elif` was dead code and g++ silently got the
    // "standard" policy.  Verified with `g++ -dM -E`.
    return kImpls[impl_index_of("libstdc++ 15")];
#else
    return kImpls[impl_index_of("standard")];
#endif
}

// ---- The presentation types, per category (phase 3). ----------------------
//
// The legal presentation types.  A type char that is legal for NO
// category is a malformed spec for EVERY argument, which is a
// DIFFERENT failure from "legal type, wrong argument".
struct LegalTypeRow {
    ArgCategory category;
    const char* types;
};

constexpr LegalTypeRow kLegalTypeTable[] = {
    {ArgCategory::Boolean,       "sbBdoxX"},
    {ArgCategory::Character,     "c?bBdoxX"},
    {ArgCategory::Integral,      "bBdoxXc"},
    {ArgCategory::FloatingPoint, "aAeEfFgG"},
    {ArgCategory::String,        "s?"},
    {ArgCategory::Pointer,       "pP"},
};

[[nodiscard]] constexpr bool is_legal_type_char(ArgCategory c, char t) noexcept {
    if (c == ArgCategory::UserDefined) return true;
    if (t == '\0') return true;
    for (const LegalTypeRow& row : kLegalTypeTable) {
        if (row.category != c) continue;
        for (const char* p = row.types; *p != '\0'; ++p) {
            if (*p == t) return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr bool is_known_type_char(char t) noexcept {
    if (t == '\0') return true;
    for (const LegalTypeRow& row : kLegalTypeTable) {
        for (const char* p = row.types; *p != '\0'; ++p) {
            if (*p == t) return true;
        }
    }
    return false;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 6 — Oct 30's FIVE MASK FUNCTIONS, carried forward VERBATIM as the
//           reference implementation.
//
// This namespace exists so the lesson can PROVE that the new table is
// exactly equivalent to the two code paths it replaces.  It is the
// oracle, not the implementation.
//
// The bug in llvm/llvm-project#106136 lives in `phase2_mask` below, in
// the `else if (c == ArgCategory::Integral) narrow = (type == 'c');`
// line.  The new table moves it into a row.
// ============================================================================

namespace petra {
namespace reference {

using detail::ArgCategory;
using detail::Opt;
using detail::OptMask;

using detail::has;
using detail::mask_of;

// libc++'s PARSE (consume) mask — `__fields_*`.
[[nodiscard]] constexpr OptMask phase1_mask(ArgCategory c) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
    case ArgCategory::Character:
    case ArgCategory::Integral:
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Locale);
    case ArgCategory::FloatingPoint:
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Precision) | mask_of(Opt::Locale);
    case ArgCategory::String:
        return mask_of(Opt::Precision);
    case ArgCategory::Pointer:
        return mask_of(Opt::ZeroPad);
    case ArgCategory::UserDefined:
        return mask_of(Opt::None);
    }
    return mask_of(Opt::None);
}

[[nodiscard]] constexpr bool is_integer_presentation_type(ArgCategory c, char t) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
        return t == 'b' || t == 'B' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    case ArgCategory::Character:
        return t == 'b' || t == 'B' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    case ArgCategory::Integral:
        return t == 'b' || t == 'B' || t == 'c' || t == 'd' || t == 'o' || t == 'x' || t == 'X';
    default:
        return false;
    }
}

// NOTE: the calls below are QUALIFIED on purpose.  Oct 30's code calls
// `is_integer_presentation_type(c, type)` unqualified, and in this file
// that name exists TWICE: Oct 30's switch-based copy (here) and the new
// table-driven copy in `petra::detail`.  Unqualified, the call would be
// ambiguous -- ordinary lookup finds this one, and argument-dependent
// lookup finds the detail one, because ArgCategory lives there.  The
// qualification keeps Oct 30's function body byte-for-byte and makes the
// oracle's independence explicit.
[[nodiscard]] constexpr OptMask phase2_mask(ArgCategory c, char type) noexcept {
    bool narrow = false;
    if (c == ArgCategory::Boolean) {
        narrow = !reference::is_integer_presentation_type(c, type);
    } else if (c == ArgCategory::Character) {
        narrow = !reference::is_integer_presentation_type(c, type);
    } else if (c == ArgCategory::Integral) {
        narrow = (type == 'c');
    }

    if (!narrow) return phase1_mask(c);
    return phase1_mask(c) & mask_of(Opt::Locale);
}

[[nodiscard]] constexpr OptMask standard_base_mask(ArgCategory c) noexcept {
    switch (c) {
    case ArgCategory::Boolean:
    case ArgCategory::Character:
        return mask_of(Opt::Locale);
    case ArgCategory::Integral:
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Locale);
    case ArgCategory::FloatingPoint:
        return mask_of(Opt::Sign) | mask_of(Opt::Alternate) | mask_of(Opt::ZeroPad)
             | mask_of(Opt::Locale) | mask_of(Opt::Precision);
    case ArgCategory::String:
        return mask_of(Opt::Precision);
    case ArgCategory::Pointer:
        return mask_of(Opt::ZeroPad);
    case ArgCategory::UserDefined:
        return mask_of(Opt::None);
    }
    return mask_of(Opt::None);
}

[[nodiscard]] constexpr OptMask standard_effective_mask(ArgCategory c, char type) noexcept {
    OptMask m = standard_base_mask(c);
    if (reference::is_integer_presentation_type(c, type)) {
        m = static_cast<OptMask>(m | mask_of(Opt::Sign) | mask_of(Opt::Alternate)
                                     | mask_of(Opt::ZeroPad));
    }
    return m;
}

enum class SpecPolicy : unsigned char {
    Standard,
    Libcxx21,
};

[[nodiscard]] constexpr OptMask effective_mask(ArgCategory c, char type,
                                                SpecPolicy policy) noexcept {
    if (policy == SpecPolicy::Standard) return standard_effective_mask(c, type);
    return phase2_mask(c, type);
}

}  // namespace reference
}  // namespace petra

// ============================================================================
// Part 7 — the spec parser and validator, now driven by the policy table.
//
// The shapes are Oct 30's.  What changed is where the masks come from:
//
//   parse_spec    takes the CONSUME mask explicitly (the parser is
//                 mask-aware, and the mask is now per-implementation);
//   validate_spec takes the ALLOW mask explicitly.
//
// Both are still constexpr.
// ============================================================================

namespace petra {
namespace detail {

enum class SpecFailure : unsigned char {
    None,
    Malformed,
    TypeNotAllowed,
    OptionNotAllowed,
    IndexOutOfRange,
    WidthArgNotInteger,
    // NEW TODAY: the number is too large for THIS implementation's
    // parser.  It is a malformed FORMAT STRING, not a bad argument:
    // the format string cannot be a format string for any arguments.
    NumberTooLarge,
};

struct SpecVerdict {
    SpecFailure failure{SpecFailure::None};
    Opt option{Opt::None};
    char type_char{'\0'};
    const char* detail{""};

    [[nodiscard]] bool accepted() const noexcept {
        return failure == SpecFailure::None;
    }
    [[nodiscard]] bool is_parse_failure() const noexcept {
        return failure == SpecFailure::Malformed
            || failure == SpecFailure::NumberTooLarge;
    }
};

[[nodiscard]] constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}
[[nodiscard]] constexpr bool is_align(char c) noexcept {
    return c == '<' || c == '>' || c == '^';
}
[[nodiscard]] constexpr bool is_sign(char c) noexcept {
    return c == '+' || c == '-' || c == ' ';
}

[[nodiscard]] constexpr const char* option_name(Opt o) noexcept {
    switch (o) {
    case Opt::Sign:      return "sign";
    case Opt::Alternate: return "alternate form";
    case Opt::ZeroPad:   return "zero-padding";
    case Opt::Precision: return "precision";
    case Opt::Locale:    return "locale-specific form";
    case Opt::None:      return "";
    }
    return "";
}

struct ParsedSpec {
    OptMask options{kMNone};
    char type_char{'\0'};
    bool has_nested{false};
    std::size_t nested_arg{0};
    bool nested_is_manual{false};
    // NEW TODAY: was an ALIGN option present?
    //
    // The align option is legal for EVERY category, so it has no bit
    // in the option mask -- but it still changes what the '0' option
    // MEANS, because [format.string.std] has two wordings for the
    // align/zero rule and the two implementations enforce different
    // ones (see ImplPolicy::zero_ignored_with_align).
    bool has_align{false};
};

// Accumulate one decimal digit into `value`, saturating at UINT64_MAX.
//
// `too_large` is set when the value has passed the implementation's
// ceiling, or when it has overflowed 64 bits -- which passes every
// ceiling there is.  With no ceiling (the standard's rules), `too_large`
// is never set and the value simply saturates.
[[nodiscard]] constexpr std::uint64_t
append_digit(std::uint64_t value, char c, bool& too_large,
             const std::optional<std::uint64_t>& max_number) noexcept {
    const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) {
        too_large = true;
        return std::numeric_limits<std::uint64_t>::max();
    }
    value = value * 10u + digit;
    if (max_number.has_value() && value > *max_number) too_large = true;
    return value;
}

// Parse one format-spec against a CONSUME mask.
//
// The GRAMMAR of a precision is checked FIRST, independently of the
// mask: a '.' with no value after it is malformed for EVERY argument,
// so it is a FormatParse, not an argument mismatch.
[[nodiscard]] constexpr ParsedSpec
parse_spec(std::string_view spec, OptMask m, SpecFailure& failure,
           Opt& bad_option,
           const std::optional<std::uint64_t>& max_number,
           const FillBehaviour& fill,
           bool zero_ignored_with_align) noexcept {
    ParsedSpec out{};
    std::size_t i = 0;
    const std::size_t n = spec.size();

    // fill-and-align.  Legal for every category.
    //
    // NEW TODAY: the fill is a CHARACTER -- a single Unicode scalar
    // value -- and not a byte.  So the scanner DECODES it, and an
    // ill-formed fill is a malformed spec unless this implementation
    // tolerates it (libstdc++ 15 substitutes U+FFFD and carries on).
    if (fill.unicode) {
        if (i < n) {
            const Utf8CodePoint cp = decode_utf8(spec.substr(i));

            // NEW TODAY: ask the fill's ONE value ONE question.  The
            // scanner does not know how the rule and the enforcement
            // interact -- the value does.
            const FillOutcome outcome = fill.outcome(cp.fault);

            if (outcome == FillOutcome::WellFormed) {
                // A well-formed scalar value.  It is the FILL if an
                // alignment option follows it; if it IS the alignment
                // option, it is that alone.
                //
                // NEW TODAY: an ALIGN option was present.  That is not
                // an option bit -- align is legal for every category
                // -- but it changes what the '0' option means, so the
                // scanner has to remember it.
                if (i + cp.length < n && is_align(spec[i + cp.length])) {
                    if (cp.value == U'{' || cp.value == U'}') {
                        failure = SpecFailure::Malformed;
                        bad_option = Opt::None;
                        return out;
                    }
                    out.has_align = true;
                    i += cp.length + 1;
                } else if (cp.length == 1 && is_align(spec[i])) {
                    out.has_align = true;
                    ++i;
                }
            } else if (outcome == FillOutcome::Fatal) {
                // This implementation makes this kind of ill-formed
                // fill FATAL, so the SPEC is malformed.
                failure = SpecFailure::Malformed;
                bad_option = Opt::None;
                return out;
            } else {
                // This implementation RECOVERS: it substitutes U+FFFD
                // for the maximal subpart.  The fill is then one code
                // point, and it must still be followed by an alignment
                // option -- otherwise the spec has no valid reading.
                //
                // NEW TODAY: the alignment option that follows the
                // substituted fill IS an alignment option, and the '0'
                // rule needs to know that.  So it must be RECORDED.
                //
                // The arc used to consume it and forget it, which meant
                // the '0' rule saw no align option at all.  That is
                // invisible in libc++ 21 (which never recovers) and
                // invisible in libstdc++ 15 (which always validity-
                // checks the '0'), so the defect survived two lessons.
                // The switch below reproduces Nov 14's scanner, so the
                // lesson can MEASURE the difference instead of asserting
                // it -- the same trick FillBehaviour::unicode plays for the
                // OLD byte-oriented rule.
                if (i + cp.length < n && is_align(spec[i + cp.length])) {
                    if (fill.recovery_keeps_align) {
                        out.has_align = true;
                    }
                    i += cp.length + 1;
                } else {
                    failure = SpecFailure::Malformed;
                    bad_option = Opt::None;
                    return out;
                }
            }
        }
    } else if (i + 1 < n && is_align(spec[i + 1])) {
        // The OLD, byte-oriented rule.  Kept so the lesson can
        // MEASURE the difference instead of asserting it.
        if (spec[i] == '{') {
            failure = SpecFailure::Malformed;
            bad_option = Opt::None;
            return out;
        }
        out.has_align = true;
        i += 2;
    } else if (i < n && is_align(spec[i])) {
        out.has_align = true;
        ++i;
    }

    // sign.
    if (i < n && is_sign(spec[i])) {
        if (!has(m, Opt::Sign)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Sign;
            return out;
        }
        out.options = out.options | mask_of(Opt::Sign);
        ++i;
    }

    // alternate form.
    if (i < n && spec[i] == '#') {
        if (!has(m, Opt::Alternate)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Alternate;
            return out;
        }
        out.options = out.options | mask_of(Opt::Alternate);
        ++i;
    }

    // zero-padding.  If the category does not accept it, the '0' stays
    // in the input and libc++ reads it as a WIDTH.
    //
    // NEW TODAY: the '0' option and an ALIGN option INTERACT, and this
    // is the first rule in the arc that only exists where TWO options
    // MEET -- which is why the 1,596-cell grid cannot see it.
    //
    // libc++ 21 implements the C++20 wording of [format.string.std]:
    // "if the 0 character and an align option both appear, the 0
    // character is ignored".  So the '0' is CONSUMED from the input,
    // but it is not an OPTION at all: it is never validity-checked, and
    // a bool accepts "{:<0}".
    //
    // The current wording (since P2572R1) keeps the option valid-or-not
    // REGARDLESS of the align option -- the align only zeroes the
    // padding count n -- so a bool still rejects "{:<0}".  That is what
    // libstdc++ 15 does.
    //
    // So the '0' is dropped from the option set only when this
    // implementation ignores it, which is the per-implementation
    // datum ImplPolicy::zero_ignored_with_align.
    if (i < n && spec[i] == '0') {
        if (!has(m, Opt::ZeroPad)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::ZeroPad;
            return out;
        }
        if (!(out.has_align && zero_ignored_with_align)) {
            out.options = out.options | mask_of(Opt::ZeroPad);
        }
        ++i;
    }

    // width.
    if (i < n && spec[i] == '0') {
        failure = SpecFailure::Malformed;
        bad_option = Opt::None;
        return out;
    }
    if (i < n && is_digit(spec[i])) {
        std::uint64_t value = 0;
        bool too_large = false;
        while (i < n && is_digit(spec[i])) {
            value = append_digit(value, spec[i], too_large, max_number);
            ++i;
        }
        if (too_large) {
            failure = SpecFailure::NumberTooLarge;
            bad_option = Opt::None;
            return out;
        }
    } else if (i < n && spec[i] == '{') {
        ++i;
        const std::size_t id_start = i;
        while (i < n && is_digit(spec[i])) ++i;
        out.has_nested = true;
        out.nested_is_manual = (i > id_start);
        out.nested_arg = 0;
        std::uint64_t nested_value = 0;
        bool nested_too_large = false;
        for (std::size_t k = id_start; k < i; ++k) {
            nested_value = append_digit(nested_value, spec[k], nested_too_large,
                                        max_number);
        }
        if (nested_too_large) {
            failure = SpecFailure::NumberTooLarge;
            bad_option = Opt::None;
            return out;
        }
        if (nested_value
            > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            failure = SpecFailure::IndexOutOfRange;
            bad_option = Opt::None;
            return out;
        }
        out.nested_arg = static_cast<std::size_t>(nested_value);
        if (i >= n || spec[i] != '}') {
            failure = SpecFailure::Malformed;
            bad_option = Opt::None;
            return out;
        }
        ++i;
    }

    // precision.
    if (i < n && spec[i] == '.') {
        ++i;
        bool well_formed = false;
        if (i < n && is_digit(spec[i])) {
            std::uint64_t value = 0;
            bool too_large = false;
            while (i < n && is_digit(spec[i])) {
                value = append_digit(value, spec[i], too_large, max_number);
                ++i;
            }
            if (too_large) {
                failure = SpecFailure::NumberTooLarge;
                bad_option = Opt::None;
                return out;
            }
            well_formed = true;
        } else if (i < n && spec[i] == '{') {
            ++i;
            const std::size_t id_start = i;
            while (i < n && is_digit(spec[i])) ++i;
            out.has_nested = true;
            out.nested_is_manual = (i > id_start);
            out.nested_arg = 0;
            std::uint64_t nested_value = 0;
            bool nested_too_large = false;
            for (std::size_t k = id_start; k < i; ++k) {
                nested_value = append_digit(nested_value, spec[k], nested_too_large,
                                            max_number);
            }
            if (nested_too_large) {
                failure = SpecFailure::NumberTooLarge;
                bad_option = Opt::None;
                return out;
            }
            if (nested_value
                > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                failure = SpecFailure::IndexOutOfRange;
                bad_option = Opt::None;
                return out;
            }
            out.nested_arg = static_cast<std::size_t>(nested_value);
            if (i >= n || spec[i] != '}') {
                failure = SpecFailure::Malformed;
                bad_option = Opt::None;
                return out;
            }
            ++i;
            well_formed = true;
        }

        if (!well_formed) {
            failure = SpecFailure::Malformed;
            bad_option = Opt::None;
            return out;
        }
        if (!has(m, Opt::Precision)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Precision;
            return out;
        }
        out.options = out.options | mask_of(Opt::Precision);
    }

    // locale-specific form.
    if (i < n && spec[i] == 'L') {
        if (!has(m, Opt::Locale)) {
            failure = SpecFailure::OptionNotAllowed;
            bad_option = Opt::Locale;
            return out;
        }
        out.options = out.options | mask_of(Opt::Locale);
        ++i;
    }

    // presentation type.
    if (i < n && spec[i] != '}') {
        out.type_char = spec[i];
        ++i;
    }

    if (i != n) {
        failure = SpecFailure::Malformed;
        bad_option = Opt::None;
        return out;
    }
    return out;
}

// Validate one parsed spec against an ALLOW mask.
[[nodiscard]] constexpr SpecVerdict
validate_spec(ArgCategory category, const ParsedSpec& spec,
              OptMask allowed) noexcept {
    SpecVerdict v{};
    v.type_char = spec.type_char;

    if (category == ArgCategory::UserDefined) {
        // We know nothing about a user's formatter.  Abstain.
        return v;
    }

    // Phase 3: the presentation type itself.
    if (!is_legal_type_char(category, spec.type_char)) {
        v.failure = is_known_type_char(spec.type_char)
                         ? SpecFailure::TypeNotAllowed
                         : SpecFailure::Malformed;
        v.detail = "the presentation type is not valid for this argument";
        return v;
    }

    // Phase 2: the mask.
    for (const Opt o : {Opt::Sign, Opt::Alternate, Opt::ZeroPad, Opt::Precision, Opt::Locale}) {
        if (!has(spec.options, o)) continue;
        if (!has(allowed, o)) {
            v.failure = SpecFailure::OptionNotAllowed;
            v.option = o;
            v.detail = "the argument does not accept this option";
            return v;
        }
    }
    return v;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 8 — the type-aware scanner (Oct 30's shape, policy-driven).
// ============================================================================

namespace petra {
namespace detail {

enum class ScanFailure : unsigned char {
    None,
    Malformed,
    TypeNotAllowed,
    OptionNotAllowed,
    IndexOutOfRange,
    WidthArgNotInteger,
    MixedNumbering,
    // NEW TODAY: the number is too large for THIS implementation.
    NumberTooLarge,
};

struct ScanResult {
    ScanFailure failure{ScanFailure::None};
    Opt option{Opt::None};
    char type_char{'\0'};
    std::size_t field{0};
    std::size_t arg_index{0};
    const char* detail{""};

    [[nodiscard]] bool ok() const noexcept {
        return failure == ScanFailure::None;
    }
    [[nodiscard]] bool is_parse_failure() const noexcept {
        return failure == ScanFailure::Malformed
            || failure == ScanFailure::MixedNumbering
            || failure == ScanFailure::NumberTooLarge;
    }
};

struct CategoryTable {
    const ArgCategory* categories{nullptr};
    std::size_t count{0};

    [[nodiscard]] constexpr ArgCategory at(std::size_t i) const noexcept {
        if (categories == nullptr || i >= count) return ArgCategory::UserDefined;
        return categories[i];
    }
    [[nodiscard]] constexpr bool known() const noexcept {
        return categories != nullptr;
    }
};

class TypeAwareScanner {
public:
    constexpr TypeAwareScanner(std::string_view fmt, CategoryTable cats,
                               const ImplPolicy& policy) noexcept
        : fmt_(fmt), cats_(cats), policy_(policy) {}

    [[nodiscard]] constexpr ScanResult run() noexcept {
        ScanResult r{};
        std::size_t i = 0;
        while (i < fmt_.size()) {
            const char c = fmt_[i];
            if (c == '{') {
                if (i + 1 < fmt_.size() && fmt_[i + 1] == '{') {
                    i += 2;
                    continue;
                }
                ++i;
                if (!scan_field(i, r)) return r;
                continue;
            }
            if (c == '}') {
                if (i + 1 < fmt_.size() && fmt_[i + 1] == '}') {
                    i += 2;
                    continue;
                }
                r.failure = ScanFailure::Malformed;
                r.detail = "a '}' that is not part of an escape sequence";
                return r;
            }
            ++i;
        }
        return r;
    }

private:
    constexpr bool parse_arg_id(std::size_t& i, bool& manual,
                                std::size_t& value) noexcept {
        const std::size_t start = i;
        while (i < fmt_.size() && is_digit(fmt_[i])) ++i;
        manual = (i > start);
        value = 0;
        arg_id_too_large_ = false;
        if (manual) {
            std::uint64_t acc = 0;
            for (std::size_t k = start; k < i; ++k) {
                acc = append_digit(acc, fmt_[k], arg_id_too_large_,
                                   policy_.max_number);
            }
            value = static_cast<std::size_t>(acc);
        }
        return true;
    }

    constexpr bool note_numbering_mode(bool manual) noexcept {
        if (manual) {
            if (auto_seen_) {
                mixed_ = true;
                return false;
            }
            manual_seen_ = true;
        } else {
            if (manual_seen_) {
                mixed_ = true;
                return false;
            }
            auto_seen_ = true;
        }
        return true;
    }

    constexpr bool scan_field(std::size_t& i, ScanResult& r) noexcept {
        const std::size_t field_no = field_count_++;

        bool manual = false;
        std::size_t index = 0;
        if (!parse_arg_id(i, manual, index)) return false;
        if (arg_id_too_large_) {
            // The index does not FIT in the implementation's parser.
            // That is a malformed FORMAT STRING -- the stdlib rejects it
            // before it looks at the argument list -- and not an index
            // that merely points past the end of it.
            r.failure = ScanFailure::NumberTooLarge;
            r.field = field_no;
            r.detail = "the argument index is too large for this implementation";
            return false;
        }
        if (!note_numbering_mode(manual)) {
            r.failure = ScanFailure::MixedNumbering;
            r.field = field_no;
            r.detail = "manual and automatic argument numbering are mixed";
            return false;
        }
        if (!manual) {
            index = next_auto_++;
        }

        if (i >= fmt_.size()) {
            r.failure = ScanFailure::Malformed;
            r.field = field_no;
            r.detail = "the replacement field has no terminator";
            return false;
        }

        if (fmt_[i] == ':') {
            ++i;
            const std::size_t spec_start = i;
            std::size_t depth = 0;
            std::size_t j = i;
            while (j < fmt_.size()) {
                if (fmt_[j] == '{') {
                    ++depth;
                } else if (fmt_[j] == '}') {
                    if (depth == 0) break;
                    --depth;
                }
                ++j;
            }
            if (j >= fmt_.size()) {
                r.failure = ScanFailure::Malformed;
                r.field = field_no;
                r.detail = "the replacement field has no terminator";
                return false;
            }
            const std::string_view spec = fmt_.substr(spec_start, j - spec_start);

            const ArgCategory cat = cats_.at(index);
            SpecFailure sf = SpecFailure::None;
            Opt bad = Opt::None;
            const ParsedSpec parsed =
                parse_spec(spec, policy_consume_mask(cat, policy_), sf, bad,
                           policy_.max_number, policy_.fill,
                           policy_.zero_ignored_with_align);
            if (sf != SpecFailure::None) {
                r.failure = to_scan_failure(sf);
                r.option = bad;
                r.field = field_no;
                r.arg_index = index;
                r.detail = (sf == SpecFailure::Malformed)
                               ? "the format spec is malformed"
                           : (sf == SpecFailure::NumberTooLarge)
                               ? "the number in the format spec is too large "
                                 "for this implementation"
                               : "the format spec is not valid for this argument";
                return false;
            }

            const SpecVerdict v =
                validate_spec(cat, parsed, policy_allow_mask(cat, parsed.type_char, policy_));
            if (!v.accepted()) {
                r.failure = to_scan_failure(v.failure);
                r.option = v.option;
                r.type_char = v.type_char;
                r.field = field_no;
                r.arg_index = index;
                r.detail = v.detail;
                return false;
            }

            if (parsed.has_nested) {
                if (parsed.nested_is_manual != manual) {
                    r.failure = ScanFailure::MixedNumbering;
                    r.field = field_no;
                    r.detail = "manual and automatic argument numbering are mixed";
                    return false;
                }

                std::size_t nested_index = parsed.nested_arg;
                if (!parsed.nested_is_manual) {
                    nested_index = next_auto_++;
                }

                if (cats_.known() && nested_index >= cats_.count) {
                    r.failure = ScanFailure::IndexOutOfRange;
                    r.field = field_no;
                    r.arg_index = nested_index;
                    r.detail = "the argument index is past the end of the argument list";
                    return false;
                }
                const ArgCategory ncat = cats_.at(nested_index);
                if (cats_.known() && ncat != ArgCategory::UserDefined
                    && ncat != ArgCategory::Integral) {
                    r.failure = ScanFailure::WidthArgNotInteger;
                    r.field = field_no;
                    r.arg_index = nested_index;
                    r.detail = "a dynamic width or precision must be an integer argument";
                    return false;
                }
            }

            i = j + 1;
            return true;
        }

        if (fmt_[i] == '}') {
            ++i;
            if (cats_.known() && index >= cats_.count) {
                r.failure = ScanFailure::IndexOutOfRange;
                r.field = field_no;
                r.arg_index = index;
                r.detail = "the argument index is past the end of the argument list";
                return false;
            }
            return true;
        }

        r.failure = ScanFailure::Malformed;
        r.field = field_no;
        r.detail = "the argument index is not terminated by ':' or '}'";
        return false;
    }

    [[nodiscard]] static constexpr ScanFailure to_scan_failure(SpecFailure sf) noexcept {
        switch (sf) {
        case SpecFailure::None:              return ScanFailure::None;
        case SpecFailure::Malformed:         return ScanFailure::Malformed;
        case SpecFailure::TypeNotAllowed:    return ScanFailure::TypeNotAllowed;
        case SpecFailure::OptionNotAllowed:  return ScanFailure::OptionNotAllowed;
        case SpecFailure::IndexOutOfRange:   return ScanFailure::IndexOutOfRange;
        case SpecFailure::WidthArgNotInteger:return ScanFailure::WidthArgNotInteger;
        case SpecFailure::NumberTooLarge:    return ScanFailure::NumberTooLarge;
        }
        return ScanFailure::Malformed;
    }

    std::string_view fmt_;
    CategoryTable cats_;
    const ImplPolicy& policy_;
    std::size_t next_auto_{0};
    std::size_t field_count_{0};
    bool manual_seen_{false};
    bool auto_seen_{false};
    bool mixed_{false};
    bool arg_id_too_large_{false};
};

[[nodiscard]] constexpr ScanResult
scan_format_string(std::string_view fmt, CategoryTable cats,
                    const ImplPolicy& policy) noexcept {
    return TypeAwareScanner{fmt, cats, policy}.run();
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 9 — the MESSAGE TABLE, carried forward from Oct 28 and DEMOTED.
//
// Oct 30's table was libc++ 21's message text, which is not part of any
// standard.  The table is unchanged; it is still the fallback for
// failures that are not about the spec's shape.
//
// Note the measurement in Part 13: libstdc++ 15's messages are a
// DIFFERENT set of strings, so under a libstdc++ build the fallback
// fires on more cells.  That is a property of the MESSAGES, not of the
// policy table, and it is exactly why the arc's own classification
// does not depend on them.
// ============================================================================

namespace petra {
namespace detail {

enum class MessageVerdict : unsigned char {
    Parse,
    Argument,
    Ambiguous,
    Unknown,
};

constexpr std::string_view kParseMessages[] = {
    "The format string terminates at a '{'",
    "The format string contains an invalid escape sequence",
    "The argument index starts with an invalid character",
    "The argument index is invalid",
    "End of input while parsing an argument index",
    "The argument index should end with a ':' or a '}'",
    "The replacement field misses a terminating '}'",
    "Using automatic argument numbering in manual argument numbering mode",
    "Using manual argument numbering in automatic argument numbering mode",
    "The precision option does not contain a value or an argument index",
    "End of input while parsing format specifier precision",
    "The fill option contains an invalid value",
    "The format specifier contains malformed Unicode characters",
    "The numeric value of the format specifier is too large",
};

constexpr std::string_view kArgumentPrefixes[] = {
    "The type option contains an invalid value for ",
    "The format specifier for ",
};

constexpr std::string_view kArgumentMessages[] = {
    "The argument index value is too large for the number of arguments "
    "supplied",
    "The value of the argument index exceeds its maximum value",
    "Argument index outside the valid range",
    "An argument index may not have a negative value",
    "Replacement argument isn't a standard signed or unsigned integer type",
    "The type does not fit in the mask",
    "Integral value outside the range of the char type",
    "Type s requires character type as formatting argument",
    "Type ?s requires character type as formatting argument",
    "Type m requires a pair or a tuple with two elements",
    "Type s and an underlying format specification can't be used together",
    "Type ?s and an underlying format specification can't be used together",
    "The n option and type s can't be used together",
    "The n option and type ?s can't be used together",
    "The format specification does not allow the sign option",
    "The format specifier does not allow the sign option",
    "The format specifier does not allow the alternate form option",
    "The format specifier does not allow the zero-padding option",
    "The format specifier does not allow the precision option",
    "The format specifier does not allow the locale-specific form option",
    "The format specifier does not allow the n option",
    "The format specifier uses an invalid value for the type option",
};

constexpr std::string_view kAmbiguousMessages[] = {
    "The format specifier should consume the input or end with a '}'",
    "The width option should not have a leading zero",
};

template <std::size_t N>
[[nodiscard]] constexpr bool
contains(const std::string_view (&table)[N], std::string_view needle) noexcept {
    for (const std::string_view entry : table) {
        if (entry == needle) return true;
    }
    return false;
}

template <std::size_t N>
[[nodiscard]] constexpr bool
starts_with_any(const std::string_view (&table)[N],
                std::string_view needle) noexcept {
    for (const std::string_view entry : table) {
        if (needle.size() >= entry.size()
            && needle.substr(0, entry.size()) == entry) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr MessageVerdict
verdict_from_message(std::string_view message) noexcept {
    if (contains(kParseMessages, message)) return MessageVerdict::Parse;
    if (contains(kArgumentMessages, message)) return MessageVerdict::Argument;
    if (starts_with_any(kArgumentPrefixes, message)) return MessageVerdict::Argument;
    if (contains(kAmbiguousMessages, message)) return MessageVerdict::Ambiguous;
    return MessageVerdict::Unknown;
}

// ============================================================================
// Part 10 — the CLASSIFIER (Oct 30's shape, policy-driven).
//
//   Stage A: the TYPE-AWARE scanner.  It answers BOTH questions — is the
//            string well-formed (FormatParse), and is the spec valid for
//            the argument it names (FormatArgument) — so it decides the
//            whole spec-vs-argument class WITHOUT reading the message.
//   Stage B: the message table, for the failures that are not about the
//            spec's shape: a value that does not fit the selected
//            presentation type, an argument index past the end of the
//            list, a user's own formatter throwing.
//   Fallback: Format.
// ============================================================================

// The two shapes of the fallback.
//
//   Discard  the old one.  The scanner's SILENCE is read as an
//            affirmation — "scanner disagrees: string is well-formed" —
//            and a parse verdict the scanner never earned is thrown
//            away.
//
//   Carry    the new one.  The scanner's silence is carried into the
//            decision AS SILENCE.  The scanner models a subset of
//            [format.string.std], so "no objection" is not "affirmed
//            well-formed", and a parse verdict from the stdlib survives
//            into FormatParse.
//
// The two differ in exactly one branch.  Keeping them side by side is
// what lets the lesson MEASURE the difference instead of asserting it.
enum class Fallback : unsigned char { Discard, Carry };

struct Classification {
    FormatErrorKind kind{FormatErrorKind::Format};
    std::string message;
    bool used_message_table{false};
    const char* reason{""};
    // The scanner's verdict, CARRIED.  Not a decision: the record of
    // what the scanner said, so that a caller (and the lesson) can see
    // whether the fallback ran at all.
    ScanFailure scanner_verdict{ScanFailure::None};
};

[[nodiscard]] inline Classification
classify_format_failure(std::string_view fmt, const char* what,
                         CategoryTable cats, const ImplPolicy& policy,
                         Fallback fallback = Fallback::Carry) noexcept {
    Classification c{};

    const ScanResult s = scan_format_string(fmt, cats, policy);
    c.scanner_verdict = s.failure;

    switch (s.failure) {
    case ScanFailure::None:
        // The scanner had NO OBJECTION.  It did not affirm anything:
        // it models a subset of the rules, and this failure is not in
        // the subset it models.  Carry that forward as silence.
        break;

    case ScanFailure::Malformed:
    case ScanFailure::MixedNumbering:
    case ScanFailure::NumberTooLarge:
        c.kind = FormatErrorKind::FormatParse;
        c.message = std::string{what} + " [format string: " + s.detail + "]";
        c.reason = s.detail;
        return c;

    case ScanFailure::TypeNotAllowed:
    case ScanFailure::OptionNotAllowed:
    case ScanFailure::IndexOutOfRange:
    case ScanFailure::WidthArgNotInteger:
        c.kind = FormatErrorKind::FormatArgument;
        c.message = std::string{what} + " [argument " + std::to_string(s.arg_index)
                    + ": " + s.detail + "]";
        c.reason = s.detail;
        return c;
    }

    c.used_message_table = true;
    switch (verdict_from_message(what)) {
    case MessageVerdict::Parse:
        if (fallback == Fallback::Carry) {
            // The stdlib says the FORMAT STRING is broken.  The scanner
            // had no opinion.  Silence is not a contradiction, so the
            // verdict survives.
            c.kind = FormatErrorKind::FormatParse;
            c.message = std::string{what} + " [the scanner had no rule for this]";
            c.reason = "the stdlib reports a parse failure the scanner did not model";
            return c;
        }
        c.kind = FormatErrorKind::Format;
        c.message = std::string{what} + " [scanner disagrees: string is well-formed]";
        c.reason = "the message says parse but the string is well-formed";
        return c;
    case MessageVerdict::Argument:
    case MessageVerdict::Ambiguous:
        c.kind = FormatErrorKind::FormatArgument;
        c.message = std::string{what};
        c.reason = "the stdlib's message identifies an argument mismatch";
        return c;
    case MessageVerdict::Unknown:
    default:
        c.kind = FormatErrorKind::Format;
        c.message = std::string{what};
        c.reason = "unrecognised message";
        return c;
    }
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 11 — the two-pass runtime implementation.
//
// Unchanged from Oct 30 apart from one thing: the classifier is now
// given the LINKED implementation's policy rather than a hard-wired
// "standard".  That is the point of the table — the same source is
// correct against whichever stdlib it is linked with.
// ============================================================================

namespace petra {
namespace detail {

template <typename... Args>
[[nodiscard]] constexpr std::array<ArgCategory, sizeof...(Args)>
categories_of() noexcept {
    return {category_of<Args>()...};
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, FormatError>
try_format_runtime_n_impl(char* out, std::size_t cap,
                          std::string_view fmt,
                          const Args&... args) {
    std::size_t needed = 0;
    auto cb = [](std::size_t n, void* data) noexcept {
        *static_cast<std::size_t*>(data) = n;
    };

    constexpr std::array<ArgCategory, sizeof...(Args)> cats = categories_of<Args...>();
    const CategoryTable table{cats.data(), cats.size()};
    const ImplPolicy& policy = linked_policy();

    try {
        counting_output_iterator counter{cb, &needed};
        (void)std::vformat_to(std::move(counter), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        const Classification c =
            classify_format_failure(fmt, e.what(), table, policy);
        return std::unexpected(FormatError{c.kind, c.message});
    }

    if (needed > cap) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"formatted output would be "}
                            + std::to_string(needed)
                            + " bytes; cap is " + std::to_string(cap),
                        needed});
    }

    std::size_t written = 0;
    try {
        bounded_char_writer sink{out, cap, cb, &written};
        (void)std::vformat_to(std::move(sink), fmt,
                              std::make_format_args(args...));
    } catch (const std::format_error& e) {
        const Classification c =
            classify_format_failure(fmt, e.what(), table, policy);
        return std::unexpected(FormatError{c.kind, c.message});
    }
    if (written != needed) {
        return std::unexpected(
            FormatError{FormatErrorKind::TooLarge,
                        std::string{"bounded_char_writer wrote "}
                            + std::to_string(written)
                            + " bytes; measure pass said "
                            + std::to_string(needed),
                        needed});
    }
    return written;
}

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 12 — the test type (a user-defined formatter) and the
//           compile-time pins.
// ============================================================================

namespace petra_test {

struct point {
    int x{0};
    int y{0};
};

}  // namespace petra_test

template <>
struct std::formatter<petra_test::point> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const petra_test::point& p, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({},{})", p.x, p.y);
    }
};

namespace petra {
namespace detail {

// The type-category table (Oct 30's pins).
static_assert(category_of<bool>() == ArgCategory::Boolean);
static_assert(category_of<char>() == ArgCategory::Character);
static_assert(category_of<signed char>() == ArgCategory::Integral);
static_assert(category_of<unsigned char>() == ArgCategory::Integral);
static_assert(category_of<short>() == ArgCategory::Integral);
static_assert(category_of<int>() == ArgCategory::Integral);
static_assert(category_of<unsigned>() == ArgCategory::Integral);
static_assert(category_of<long>() == ArgCategory::Integral);
static_assert(category_of<long long>() == ArgCategory::Integral);
static_assert(category_of<float>() == ArgCategory::FloatingPoint);
static_assert(category_of<double>() == ArgCategory::FloatingPoint);
static_assert(category_of<long double>() == ArgCategory::FloatingPoint);
static_assert(category_of<const char*>() == ArgCategory::String);
static_assert(category_of<char*>() == ArgCategory::String);
static_assert(category_of<char[4]>() == ArgCategory::String);
static_assert(category_of<std::string>() == ArgCategory::String);
static_assert(category_of<std::string_view>() == ArgCategory::String);
static_assert(category_of<void*>() == ArgCategory::Pointer);
static_assert(category_of<const void*>() == ArgCategory::Pointer);
static_assert(category_of<std::nullptr_t>() == ArgCategory::Pointer);
static_assert(category_of<wchar_t>() == ArgCategory::UserDefined);
static_assert(category_of<char16_t>() == ArgCategory::UserDefined);
static_assert(category_of<petra_test::point>() == ArgCategory::UserDefined);

// The base table.
static_assert(!has(base_mask(ArgCategory::Boolean), Opt::Sign));
static_assert(has(base_mask(ArgCategory::Boolean), Opt::Locale));
static_assert(!has(base_mask(ArgCategory::Character), Opt::Sign));
static_assert(has(base_mask(ArgCategory::Character), Opt::Locale));
static_assert(has(base_mask(ArgCategory::Integral), Opt::Sign));
static_assert(has(base_mask(ArgCategory::FloatingPoint), Opt::Precision));
static_assert(!has(base_mask(ArgCategory::Integral), Opt::Precision));
static_assert(has(base_mask(ArgCategory::String), Opt::Precision));
static_assert(!has(base_mask(ArgCategory::String), Opt::Sign));
static_assert(has(base_mask(ArgCategory::Pointer), Opt::ZeroPad));
static_assert(!has(base_mask(ArgCategory::Pointer), Opt::Sign));

// The integer-presentation types.
static_assert(is_integer_presentation_type(ArgCategory::Integral, 'c'));
static_assert(!is_integer_presentation_type(ArgCategory::Character, 'c'));
static_assert(!is_integer_presentation_type(ArgCategory::Boolean, 'c'));
static_assert(is_integer_presentation_type(ArgCategory::Boolean, 'd'));
static_assert(is_integer_presentation_type(ArgCategory::Character, 'x'));

// THE DIVERGENCE, pinned from both sides.
static_assert(has(allow_mask(ArgCategory::Integral, 'c'), Opt::Sign));
static_assert(!has(policy_allow_mask(ArgCategory::Integral, 'c',
                                      kImpls[impl_index_of("libc++ 21")]),
                    Opt::Sign));
static_assert(has(policy_allow_mask(ArgCategory::Integral, 'c',
                                     kImpls[impl_index_of("libc++ 21")]),
                   Opt::Locale));
// charT is NOT divergent: both policies narrow it.
static_assert(!has(allow_mask(ArgCategory::Character, 'c'), Opt::Sign));
static_assert(!has(policy_allow_mask(ArgCategory::Character, 'c',
                                     kImpls[impl_index_of("libc++ 21")]),
                    Opt::Sign));
// bool is NOT divergent.
static_assert(!has(allow_mask(ArgCategory::Boolean, 's'), Opt::Sign));
static_assert(!has(policy_allow_mask(ArgCategory::Boolean, 's',
                                     kImpls[impl_index_of("libc++ 21")]),
                    Opt::Sign));
static_assert(has(allow_mask(ArgCategory::Boolean, 'd'), Opt::Sign));
static_assert(has(policy_allow_mask(ArgCategory::Boolean, 'd',
                                     kImpls[impl_index_of("libc++ 21")]),
                   Opt::Sign));
// libstdc++ 15's table is EMPTY, so it agrees with the standard.
static_assert(kImpls[impl_index_of("libstdc++ 15")].allow_count == 0);

// The legal presentation types.
static_assert(is_legal_type_char(ArgCategory::Boolean, 's'));
static_assert(!is_legal_type_char(ArgCategory::Boolean, 'c'));
static_assert(is_legal_type_char(ArgCategory::Character, 'c'));
static_assert(is_legal_type_char(ArgCategory::Character, '?'));
static_assert(is_legal_type_char(ArgCategory::Integral, 'c'));
static_assert(!is_legal_type_char(ArgCategory::Integral, 's'));
static_assert(is_legal_type_char(ArgCategory::FloatingPoint, 'e'));
static_assert(!is_legal_type_char(ArgCategory::FloatingPoint, 'd'));
static_assert(is_legal_type_char(ArgCategory::String, 's'));
static_assert(is_legal_type_char(ArgCategory::String, '?'));
static_assert(is_legal_type_char(ArgCategory::Pointer, 'p'));
static_assert(is_legal_type_char(ArgCategory::Pointer, 'P'));
static_assert(!is_legal_type_char(ArgCategory::Pointer, 'x'));
static_assert(!is_known_type_char('z'));
static_assert(is_known_type_char('c'));

}  // namespace detail
}  // namespace petra

// ============================================================================
// Part 13 — the harness.
//
// Section 1  the policy table: the base table, the int-pres table, the
//            consume table, and the per-impl override rows
// Section 2  EQUIVALENCE: the new table against Oct 30's five mask
//            functions, over all 7 x 19 x 2 cells
// Section 3  the 1,596-cell grid: model vs the LINKED stdlib
// Section 4  DERIVATION: the override table derived from the measured
//            grid, checked against the hand-written table
// Section 5  the consume mask vs the allow mask: what each one changes
// Section 6  the arc's own paths, end to end
// Section 7  the numeric ceiling: measured on the linked stdlib,
//            then checked against the hand-written policy row
// Section 8  THE CENSUS: the carried verdict against the discarding
//            fallback, over the grid and over the families the grid
//            omits
// Section 9  the arc's own paths, extended
// ============================================================================

namespace {

using namespace petra;
using namespace petra::detail;

constexpr std::size_t kPrefixCount = 12;
constexpr std::size_t kTypeCount = 19;
constexpr std::size_t kRowCount = 7;
constexpr std::size_t kCellCount = kPrefixCount * kTypeCount;   // 228
constexpr std::size_t kGridCells = kRowCount * kCellCount;       // 1596

// The grid's shape is a fact about the GRID, so it is checked once,
// here — not typed into six different assertions further down.
static_assert(kCellCount == 228, "12 prefixes x 19 type chars");
static_assert(kGridCells == 1596, "7 rows x 228 cells");

constexpr const char* kPrefixes[] = {
    "", "+", "-", " ", "#", "0", "10", ".2", "L", "<", "*>10", "<10.2",
};

struct TypeChar {
    const char* text;
    char ch;
};

constexpr TypeChar kTypeChars[] = {
    {"", '\0'}, {"a", 'a'}, {"A", 'A'}, {"b", 'b'}, {"B", 'B'}, {"c", 'c'},
    {"d", 'd'}, {"e", 'e'}, {"f", 'f'}, {"F", 'F'}, {"g", 'g'}, {"G", 'G'},
    {"o", 'o'}, {"p", 'p'}, {"s", 's'}, {"x", 'x'}, {"X", 'X'}, {"P", 'P'},
    {"?", '?'},
};

// The index of a prefix in kPrefixes, found at compile time.
[[nodiscard]] constexpr std::size_t prefix_index(const char* p) noexcept {
    for (std::size_t i = 0; i < kPrefixCount; ++i) {
        std::size_t j = 0;
        for (; kPrefixes[i][j] != '\0' && p[j] != '\0'; ++j) {
            if (kPrefixes[i][j] != p[j]) break;
        }
        if (kPrefixes[i][j] == '\0' && p[j] == '\0') return i;
    }
    return kPrefixCount;
}

constexpr std::size_t kSignPrefix     = prefix_index("+");
constexpr std::size_t kAltPrefix      = prefix_index("#");
constexpr std::size_t kZeroPrefix     = prefix_index("0");
constexpr std::size_t kPrecPrefix     = prefix_index(".2");
constexpr std::size_t kLocalePrefix   = prefix_index("L");
constexpr std::size_t kEmptyPrefix    = prefix_index("");

static_assert(kSignPrefix != kPrefixCount);
static_assert(kAltPrefix != kPrefixCount);
static_assert(kZeroPrefix != kPrefixCount);
static_assert(kPrecPrefix != kPrefixCount);
static_assert(kLocalePrefix != kPrefixCount);
static_assert(kEmptyPrefix == 0);

struct counters {
    std::size_t pass{0};
    std::size_t fail{0};

    void check(bool ok) noexcept {
        if (ok) ++pass; else ++fail;
    }
    void expect_eq(std::size_t got, std::size_t want, const char* what) {
        if (got == want) {
            ++pass;
        } else {
            ++fail;
            std::printf("  FAIL %s: got %zu, want %zu\n", what, got, want);
        }
    }
};

void section(const char* title) {
    std::printf("\n=== %s ===\n", title);
}

// The grid rows: one per representative argument.  Rows 2 and 3 are
// both the INTEGRAL category, which is why the divergence shows up in
// two rows of the grid and one cell of the matrix.
struct GridRow {
    const char* label;
    ArgCategory category;
};

constexpr GridRow kRows[kRowCount] = {
    {"bool",   ArgCategory::Boolean},
    {"char",   ArgCategory::Character},
    {"int",    ArgCategory::Integral},
    {"long",   ArgCategory::Integral},
    {"double", ArgCategory::FloatingPoint},
    {"string", ArgCategory::String},
    {"voidp",  ArgCategory::Pointer},
};

[[nodiscard]] std::string spec_text(std::size_t prefix_i, std::size_t type_i) {
    return std::string{kPrefixes[prefix_i]} + std::string{kTypeChars[type_i].text};
}

enum class Outcome { Ok, Reject };

template <typename T>
Outcome measure(const std::string& fmt, T& value, std::string& message) {
    try {
        (void)std::vformat(fmt, std::make_format_args(value));
        return Outcome::Ok;
    } catch (const std::format_error& e) {
        message = e.what();
        return Outcome::Reject;
    }
}

// The model's verdict for one spec against one category, under one
// policy.  `reason` names the failing option, for the diagnostics.
bool model_verdict(ArgCategory cat, std::string_view spec,
                    const ImplPolicy& policy, std::string& reason) {
    SpecFailure sf = SpecFailure::None;
    Opt bad = Opt::None;
    const ParsedSpec parsed =
        parse_spec(spec, policy_consume_mask(cat, policy), sf, bad,
                   policy.max_number, policy.fill,
                   policy.zero_ignored_with_align);
    if (sf != SpecFailure::None) {
        reason = (sf == SpecFailure::Malformed) ? "malformed" : "option not allowed";
        return false;
    }
    const SpecVerdict v =
        validate_spec(cat, parsed, policy_allow_mask(cat, parsed.type_char, policy));
    if (!v.accepted()) {
        reason = v.detail;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Section 1 — the policy table.
// ---------------------------------------------------------------------------
void section1(counters& c) {
    section("Section 1 — the policy table");

    std::printf("  TABLE 1: the standard's BASE permissions, per category\n");
    for (const BaseRow& row : kBaseTable) {
        std::string m;
        if (has(row.base, Opt::Sign))     m += "sign ";
        if (has(row.base, Opt::Alternate)) m += "alt ";
        if (has(row.base, Opt::ZeroPad))   m += "zero ";
        if (has(row.base, Opt::Precision)) m += "prec ";
        if (has(row.base, Opt::Locale))    m += "locale ";
        std::printf("    %-16s %s\n",
                    std::string{category_name(row.category)}.c_str(), m.c_str());
    }

    std::printf("\n  TABLE 2: the integer-presentation types, per category\n");
    for (const IntPresRow& row : kIntPresTable) {
        std::printf("    %-16s \"%s\"\n",
                    std::string{category_name(row.category)}.c_str(), row.types);
    }

    std::printf("\n  TABLE 3: the CONSUME mask, per category\n");
    for (const ConsumeRow& row : kSharedConsumeTable) {
        std::string m;
        if (has(row.mask, Opt::Sign))     m += "sign ";
        if (has(row.mask, Opt::Alternate)) m += "alt ";
        if (has(row.mask, Opt::ZeroPad))   m += "zero ";
        if (has(row.mask, Opt::Precision)) m += "prec ";
        if (has(row.mask, Opt::Locale))    m += "locale ";
        std::printf("    %-16s %s\n",
                    std::string{category_name(row.category)}.c_str(), m.c_str());
    }

    std::printf("\n  TABLE 4: the ALLOW override rows, per implementation\n");
    for (std::size_t i = 0; i < kImplCount; ++i) {
        const ImplPolicy& impl = kImpls[i];
        std::printf("    %-14s rows=%zu\n", impl.name, impl.allow_count);
        for (std::size_t r = 0; r < impl.allow_count; ++r) {
            const AllowRow& row = impl.allow_rows[r];
            std::printf("      -> category=%-10s type=%-4s mask=%s\n",
                        std::string{category_name(row.category)}.c_str(),
                        row.any_type ? "any" : (row.type_char == '\0'
                                                 ? "(none)" :
                                                 std::string(1, row.type_char).c_str()),
                        row.mask == kMLocale ? "locale" : "(other)");
        }
    }

    std::printf("\n  the linked implementation is \"%s\"\n", linked_policy().name);
    std::printf("  (so the arc models the stdlib it is actually linked with,\n"
                "   and the SAME source is correct under both toolchains.)\n");

    c.expect_eq(kImpls[impl_index_of("libc++ 21")].allow_count, 1,
                "libc++ 21 override rows");
    c.expect_eq(kImpls[impl_index_of("standard")].allow_count, 0,
                "standard override rows");
    c.expect_eq(kImpls[impl_index_of("libstdc++ 15")].allow_count, 0,
                "libstdc++ 15 override rows");
    c.check(linked_policy().name != nullptr);
}

// ---------------------------------------------------------------------------
// Section 2 — EQUIVALENCE with Oct 30's five mask functions.
//
// The new table must agree with the old code paths on EVERY cell:
// 7 categories x 19 type chars x 2 policies = 266 comparisons, plus the
// 7 consume masks.
// ---------------------------------------------------------------------------
void section2(counters& c) {
    section("Section 2 — EQUIVALENCE: the table vs Oct 30's five mask functions");

    std::size_t compared = 0;
    std::size_t mismatches = 0;

    for (const GridRow& row : kRows) {
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            const char type = kTypeChars[t].ch;

            const OptMask std_new =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("standard")]);
            const OptMask std_old =
                reference::standard_effective_mask(row.category, type);
            ++compared;
            if (std_new != std_old) {
                ++mismatches;
                std::printf("  MISMATCH standard %s type '%c': table=%u old=%u\n",
                            std::string{category_name(row.category)}.c_str(), type,
                            static_cast<unsigned>(std_new),
                            static_cast<unsigned>(std_old));
            }

            const OptMask libc_new =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("libc++ 21")]);
            const OptMask libc_old = reference::phase2_mask(row.category, type);
            ++compared;
            if (libc_new != libc_old) {
                ++mismatches;
                std::printf("  MISMATCH libc++ 21 %s type '%c': table=%u old=%u\n",
                            std::string{category_name(row.category)}.c_str(), type,
                            static_cast<unsigned>(libc_new),
                            static_cast<unsigned>(libc_old));
            }
        }
    }

    std::size_t consume_compared = 0;
    for (const GridRow& row : kRows) {
        const OptMask new_c = policy_consume_mask(row.category, linked_policy());
        const OptMask old_c = reference::phase1_mask(row.category);
        ++consume_compared;
        if (new_c != old_c) {
            ++mismatches;
            std::printf("  MISMATCH consume %s: table=%u old=%u\n",
                        std::string{category_name(row.category)}.c_str(),
                        static_cast<unsigned>(new_c), static_cast<unsigned>(old_c));
        }
    }

    std::printf("  allow-mask cells compared: %zu  (7 categories x 19 types x 2 policies)\n",
                compared);
    std::printf("  consume masks compared: %zu\n", consume_compared);
    std::printf("  mismatches: %zu\n", mismatches);
    std::printf("  -> the table is EXACTLY equivalent to the two code paths it\n"
                "     replaces.  Not \"close\": every cell.\n");

    // Now the shape of the difference, which is the actual finding.
    std::printf("\n  the cells where the two policies DISAGREE:\n");
    std::size_t divergent_cells = 0;
    for (const GridRow& row : kRows) {
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            const char type = kTypeChars[t].ch;
            const OptMask a =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("standard")]);
            const OptMask b =
                policy_allow_mask(row.category, type, kImpls[impl_index_of("libc++ 21")]);
            if (a == b) continue;
            ++divergent_cells;
            std::printf("    category=%-10s type='%c'  standard=[%s]  libc++=[%s]\n",
                        std::string{category_name(row.category)}.c_str(), type,
                        mask_text(a).c_str(), mask_text(b).c_str());
        }
    }
    std::printf("  -> %zu divergent cell(s) out of %zu.\n", divergent_cells, compared);

    c.expect_eq(compared, 266, "allow-mask cells compared");
    c.expect_eq(consume_compared, 7, "consume masks compared");
    c.expect_eq(mismatches, 0, "equivalence mismatches");
    c.expect_eq(divergent_cells, 2, "divergent cells (integral and long are both Integral)");
}

// ---------------------------------------------------------------------------
// Section 3 — the 1,596-cell grid, measured against the LINKED stdlib.
// ---------------------------------------------------------------------------
void section3(counters& c) {
    section("Section 3 — the 1,596-cell grid vs the LINKED stdlib");

    const ImplPolicy& linked = linked_policy();

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    std::size_t libc_ok = 0;
    std::size_t libc_reject = 0;
    std::size_t agree_standard = 0;
    std::size_t agree_libcxx = 0;

    // The cells where the two POLICIES disagree.  This is the
    // BUILD-INVARIANT quantity: the same 10 cells under libc++ and
    // under libstdc++.  (The draft measured model(standard) against
    // the LINKED STDLIB instead, which is 10 cells under libc++ and 0
    // under libstdc++ — a build-dependent number masquerading as a
    // fact about the rules.)
    std::vector<std::string> divergent;

    // Of those divergent cells, how many the LINKED STDLIB sided with
    // libc++ 21's model rather than the standard's model.  This one IS
    // build-dependent, and it is the mirror image of the two agreement
    // counts above.
    std::size_t stdlib_sides_with_libcxx = 0;

    const auto run = [&](const char* label, ArgCategory cat, auto& value) {
        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string fmt = "{:" + spec_text(p, t) + "}";
                std::string message;
                const Outcome outcome = measure(fmt, value, message);
                if (outcome == Outcome::Ok) ++libc_ok; else ++libc_reject;

                std::string reason;
                const bool model_std =
                    model_verdict(cat, spec_text(p, t),
                                  kImpls[impl_index_of("standard")], reason);
                const bool model_libc =
                    model_verdict(cat, spec_text(p, t),
                                  kImpls[impl_index_of("libc++ 21")], reason);

                if (model_std == (outcome == Outcome::Ok)) ++agree_standard;
                if (model_libc == (outcome == Outcome::Ok)) ++agree_libcxx;

                // The DIVERGENCE is between the two POLICIES, not
                // between a policy and the linked stdlib.  The linked
                // stdlib only tells us which side the BUILD happens
                // to be on.
                if (model_std != model_libc) {
                    const bool stdlib_agrees_with_libcxx =
                        (model_libc == (outcome == Outcome::Ok));
                    if (stdlib_agrees_with_libcxx) ++stdlib_sides_with_libcxx;
                    divergent.push_back(std::string{label} + "  spec=\""
                                        + spec_text(p, t) + "\"  standard="
                                        + (model_std ? "ACCEPT" : "REJECT")
                                        + "  libc++ 21=" + (model_libc ? "ACCEPT" : "REJECT")
                                        + "  linked stdlib says "
                                        + (outcome == Outcome::Ok ? "OK" : "REJECT"));
                }
            }
        }
    };

    run("bool",   ArgCategory::Boolean,       b);
    run("char",   ArgCategory::Character,     ch);
    run("int",    ArgCategory::Integral,      i);
    run("long",   ArgCategory::Integral,      l);
    run("double", ArgCategory::FloatingPoint, d);
    run("string", ArgCategory::String,        sv);
    run("voidp",  ArgCategory::Pointer,       vp);

    const std::size_t total = libc_ok + libc_reject;

    std::printf("  linked stdlib: %s\n", linked.name);
    std::printf("  grid cells: %zu   stdlib outcomes: OK=%zu REJECT=%zu\n",
                total, libc_ok, libc_reject);
    std::printf("  model(standard)  vs stdlib: %zu/%zu agree\n", agree_standard, total);
    std::printf("  model(libc++ 21) vs stdlib: %zu/%zu agree\n", agree_libcxx, total);

    if (!divergent.empty()) {
        std::printf("\n  the cells where the two POLICIES disagree:\n");
        for (const std::string& div : divergent) {
            std::printf("    %s\n", div.c_str());
        }
        std::printf("    -> the known upstream divergence: 'c' IS an integer\n"
                    "       presentation type for integral (Table 107), so the\n"
                    "       sign / alternate / zero options are valid there.\n");
    }

    std::printf("\n  of those %zu cells, the linked stdlib sides with libc++ 21's\n"
                "  model on %zu of them, and with the standard's model on %zu.\n",
                divergent.size(), stdlib_sides_with_libcxx,
                divergent.size() - stdlib_sides_with_libcxx);

    std::printf("\n  The same source, built against the other stdlib, produces the\n"
                "  mirror image of these numbers.  Which implementation is\n"
                "  'the stdlib' is a property of the BUILD; the 10 cells are a\n"
                "  property of the RULES.\n");

    c.expect_eq(total, kGridCells, "grid cell count");
    // BUILD-INVARIANT: the two policies disagree on exactly 10 cells,
    // and it is the SAME 10 under either toolchain.
    c.expect_eq(divergent.size(), 10, "policy-vs-policy divergent cells");
    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        c.expect_eq(agree_libcxx, kGridCells, "model(libc++ 21) vs libc++");
        c.expect_eq(agree_standard, kGridCells - 10, "model(standard) vs libc++");
        c.expect_eq(stdlib_sides_with_libcxx, 10, "libc++ sides with libc++ 21");
    } else {
        c.expect_eq(agree_standard, kGridCells, "model(standard) vs libstdc++");
        c.expect_eq(agree_libcxx, kGridCells - 10, "model(libc++ 21) vs libstdc++");
        c.expect_eq(stdlib_sides_with_libcxx, 0, "libstdc++ sides with the standard");
    }
}

// ---------------------------------------------------------------------------
// Section 4 — DERIVATION.
//
// The override table is derived from the measured grid, not asserted.
// For each (category, type) cell the lesson asks: what mask would make
// the model agree with the stdlib on every prefix?  The answer is
// derived, then checked against the hand-written table.
//
// This is what makes "adding a third implementation is a data change"
// an operation rather than a slogan.
// ---------------------------------------------------------------------------
void section4(counters& c) {
    section("Section 4 — DERIVATION: the override table, derived from the measurement");

    const ImplPolicy& linked = linked_policy();

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    // For each grid row and type char, the DERIVED allow mask: the set
    // of options the stdlib accepts for that (category, type) pair.
    //
    // A cell is "legal" if the stdlib accepts the bare type char for
    // that argument; if it does not, the type is not legal for the
    // category and no mask can explain the row.
    struct Derived {
        bool legal{false};
        OptMask mask{kMNone};
    };
    Derived derived[kRowCount][kTypeCount];

    const auto derive = [&](const char* label, std::size_t row, auto& value) {
        (void)label;
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            const std::string fmt = "{:" + std::string{kTypeChars[t].text} + "}";
            std::string message;
            const Outcome outcome = measure(fmt, value, message);
            if (outcome != Outcome::Ok) continue;   // type not legal here
            derived[row][t].legal = true;

            OptMask m = kMNone;
            const std::pair<std::size_t, Opt> probes[] = {
                {kSignPrefix, Opt::Sign},
                {kAltPrefix,  Opt::Alternate},
                {kZeroPrefix, Opt::ZeroPad},
                {kPrecPrefix, Opt::Precision},
                {kLocalePrefix, Opt::Locale},
            };
            for (const auto& [prefix_i, opt] : probes) {
                const std::string spec = std::string{kPrefixes[prefix_i]}
                                      + std::string{kTypeChars[t].text};
                const std::string f = "{:" + spec + "}";
                std::string msg;
                if (measure(f, value, msg) == Outcome::Ok) m = m | mask_of(opt);
            }
            derived[row][t].mask = m;
        }
    };

    derive("bool",   0, b);
    derive("char",   1, ch);
    derive("int",    2, i);
    derive("long",   3, l);
    derive("double", 4, d);
    derive("string", 5, sv);
    derive("voidp",  6, vp);

    // Rows 2 and 3 are both the INTEGRAL category: their derived masks
    // must agree, or the model is not category-shaped.
    std::size_t row_disagreements = 0;
    for (std::size_t t = 0; t < kTypeCount; ++t) {
        if (derived[2][t].legal != derived[3][t].legal
            || derived[2][t].mask != derived[3][t].mask) {
            ++row_disagreements;
        }
    }
    std::printf("  the int row and the long row are both INTEGRAL: their\n"
                "  derived masks agree on %zu of %zu type chars\n",
                kTypeCount - row_disagreements, kTypeCount);

    // The DERIVED override table: the cells where the standard's model
    // disagrees with the measurement.
    //
    // The table is keyed by (category, type), NOT by grid row.  Rows 2
    // and 3 are both the INTEGRAL category, so they measure the SAME
    // matrix cell twice; the table must still hold ONE row for it.
    // That is the difference between a table and a log.
    std::vector<std::string> derived_rows;
    std::vector<AllowRow> overrides;
    std::vector<std::pair<ArgCategory, char>> emitted;

    for (std::size_t row = 0; row < kRowCount; ++row) {
        const ArgCategory cat = kRows[row].category;
        for (std::size_t t = 0; t < kTypeCount; ++t) {
            if (!derived[row][t].legal) continue;
            const char type = kTypeChars[t].ch;
            const OptMask std_mask = allow_mask(cat, type);
            if (std_mask == derived[row][t].mask) continue;

            bool already = false;
            for (const auto& e : emitted) {
                if (e.first == cat && e.second == type) { already = true; break; }
            }
            if (already) continue;
            emitted.emplace_back(cat, type);

            derived_rows.push_back(
                std::string{"{ category = "} + std::string{category_name(cat)}
                + ", type = '" + std::string(1, type == '\0' ? ' ' : type)
                + "', mask = " + (derived[row][t].mask == kMLocale ? "locale" : "other")
                + " }   // measured on the " + kRows[row].label + " row");
            overrides.push_back(
                AllowRow{cat, type, false, derived[row][t].mask});
        }
    }

    std::printf("\n  DERIVED override rows for \"%s\": %zu\n", linked.name,
                derived_rows.size());
    for (const std::string& r : derived_rows) {
        std::printf("    %s\n", r.c_str());
    }
    if (derived_rows.empty()) {
        std::printf("    (none: this implementation's allow matrix IS the\n"
                    "     standard's, cell for cell)\n");
    }

    // Now the check that matters: the derived table must REPRODUCE the
    // whole measured grid.
    //
    // The derived table is expressed as a per-(category, type) override
    // list, and the model is re-run with it.
    const ImplPolicy derived_policy{
        "derived", overrides.data(), overrides.size(),
        kSharedConsumeTable, 6,
        linked_policy().max_number, linked_policy().ceiling_note,
        FillBehaviour{}, linked_policy().zero_ignored_with_align};

    std::size_t agree_derived = 0;
    std::size_t total = 0;
    const auto rerun = [&](ArgCategory cat, auto& value) {
        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string fmt = "{:" + spec_text(p, t) + "}";
                std::string message;
                const Outcome outcome = measure(fmt, value, message);
                std::string reason;
                const bool model =
                    model_verdict(cat, spec_text(p, t), derived_policy, reason);
                ++total;
                if (model == (outcome == Outcome::Ok)) ++agree_derived;
            }
        }
    };
    rerun(ArgCategory::Boolean,       b);
    rerun(ArgCategory::Character,     ch);
    rerun(ArgCategory::Integral,      i);
    rerun(ArgCategory::Integral,      l);
    rerun(ArgCategory::FloatingPoint, d);
    rerun(ArgCategory::String,        sv);
    rerun(ArgCategory::Pointer,       vp);

    std::printf("\n  the DERIVED table, re-run against the measured grid:\n");
    std::printf("    agree %zu/%zu\n", agree_derived, total);

    // And the derived table must MATCH the hand-written table.
    const ImplPolicy& handwritten = linked_policy();
    std::size_t row_mismatch = 0;
    if (handwritten.allow_count != overrides.size()) {
        ++row_mismatch;
        std::printf("    row-count differs: hand-written %zu, derived %zu\n",
                    handwritten.allow_count, overrides.size());
    }
    for (std::size_t r = 0; r < handwritten.allow_count && r < overrides.size(); ++r) {
        if (handwritten.allow_rows[r].category != overrides[r].category
            || handwritten.allow_rows[r].type_char != overrides[r].type_char
            || handwritten.allow_rows[r].mask != overrides[r].mask) {
            ++row_mismatch;
            std::printf("    row %zu differs: hand-written {%s, '%c', %u} "
                        "derived {%s, '%c', %u}\n", r,
                        std::string{category_name(handwritten.allow_rows[r].category)}.c_str(),
                        handwritten.allow_rows[r].type_char,
                        static_cast<unsigned>(handwritten.allow_rows[r].mask),
                        std::string{category_name(overrides[r].category)}.c_str(),
                        overrides[r].type_char,
                        static_cast<unsigned>(overrides[r].mask));
        }
    }

    std::printf("    hand-written vs derived: %zu mismatch(es)\n", row_mismatch);
    std::printf("    -> the table is DERIVED from a measurement and CHECKED\n"
                "       against the hand-written one.  A number in a comment\n"
                "       is not a check; a number the program computes is.\n");

    c.expect_eq(row_disagreements, 0, "int/long derived-mask agreement");
    c.expect_eq(agree_derived, kGridCells, "derived table vs measured grid");
    c.expect_eq(row_mismatch, 0, "derived vs hand-written override table");
    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        c.expect_eq(derived_rows.size(), 1, "libc++ 21 derived override rows");
    } else {
        c.expect_eq(derived_rows.size(), 0, "libstdc++ 15 derived override rows");
    }
}

// ---------------------------------------------------------------------------
// Section 5 — the consume mask vs the allow mask.
//
// What does each mask actually change?
// ---------------------------------------------------------------------------
void section5(counters& c) {
    section("Section 5 — the consume mask is a diagnostic knob; the allow mask decides");

    // The "everything" consume mask: the parser consumes every option
    // it knows about, whatever the category.
    const OptMask kConsumeEverything =
        kMSign | kMAlternate | kMZero | kMPrecision | kMLocale;

    // A consume table in which EVERY category consumes every option.
    // UserDefined is deliberately absent: an unknown category has no
    // opinion, so its consume mask stays empty.
    constexpr ConsumeRow kWideConsumeTable[] = {
        {ArgCategory::Boolean,       kConsumeEverything},
        {ArgCategory::Character,     kConsumeEverything},
        {ArgCategory::Integral,      kConsumeEverything},
        {ArgCategory::FloatingPoint, kConsumeEverything},
        {ArgCategory::String,        kConsumeEverything},
        {ArgCategory::Pointer,       kConsumeEverything},
    };

    // Two policies that differ ONLY in their consume mask.  Neither has
    // an override row, so both resolve the same ALLOW mask (the
    // standard's) — which is the whole point of the comparison.
    const ImplPolicy narrow_consume{
        "narrow-consume", nullptr, 0, kSharedConsumeTable, 6};
    const ImplPolicy wide_consume{
        "wide-consume", nullptr, 0, kWideConsumeTable, 6};

    // The PUBLIC contract is what a caller sees:
    //
    //   petra::FormatErrorKind — 4 kinds, unchanged since Sep 6.
    //   the diagnostic DETAIL  — the scanner's detail string, which the
    //                            classifier embeds verbatim in the
    //                            error message.
    //
    // The arc ALSO has an internal failure enum (6 values) that the
    // classifier maps onto the public kinds.  Section 5 measures all
    // three, because they do NOT move together: the consume mask moves
    // the internal enum and the detail, and leaves the public kind
    // exactly where it was.
    std::size_t verdict_diff = 0;
    std::size_t kind_diff = 0;
    std::size_t detail_diff = 0;
    std::size_t internal_diff = 0;
    std::size_t total = 0;
    std::size_t rejected = 0;

    // The stdlib's message.  It is irrelevant to the KIND for a
    // scan-failure cell — stage A of the classifier decides the kind
    // structurally — and it is the SAME on both sides, so it cannot
    // bias the comparison.
    const char* kWhat = "the format specifier is invalid";

    const auto compare = [&](ArgCategory cat) {
        // One argument in the list, of the category under test, so the
        // scanner can resolve the field's category.
        const ArgCategory one[] = {cat};
        const CategoryTable table{one, 1};

        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string spec = spec_text(p, t);
                const std::string fmt = "{:" + spec + "}";

                // The arc's OWN scanner, once per consume policy.
                const ScanResult narrow =
                    scan_format_string(fmt, table, narrow_consume);
                const ScanResult wide =
                    scan_format_string(fmt, table, wide_consume);
                ++total;

                // (1) The VERDICT: is the spec still accepted?
                if (narrow.ok() != wide.ok()) { ++verdict_diff; continue; }
                if (narrow.ok()) continue;   // accepted by both: no error
                ++rejected;

                // (2) The PUBLIC kind, read from the arc's OWN
                //     classifier — not from a second copy of the
                //     internal->public mapping.
                const Classification cn = classify_format_failure(
                    fmt, kWhat, table, narrow_consume);
                const Classification cw = classify_format_failure(
                    fmt, kWhat, table, wide_consume);
                if (cn.kind != cw.kind) ++kind_diff;

                // (3) The PUBLIC diagnostic detail.
                if (std::strcmp(narrow.detail, wide.detail) != 0) ++detail_diff;

                // (4) The INTERNAL enum.  Reported, not asserted stable.
                if (narrow.failure != wide.failure) ++internal_diff;
            }
        }
    };

    compare(ArgCategory::Boolean);
    compare(ArgCategory::Character);
    compare(ArgCategory::Integral);
    compare(ArgCategory::Integral);
    compare(ArgCategory::FloatingPoint);
    compare(ArgCategory::String);
    compare(ArgCategory::Pointer);

    std::printf("  cells compared: %zu   (rejected by both: %zu)\n", total, rejected);
    std::printf("  accept/reject VERDICT changed:      %zu\n", verdict_diff);
    std::printf("  PUBLIC FormatErrorKind changed:      %zu\n", kind_diff);
    std::printf("  public diagnostic DETAIL changed:    %zu\n", detail_diff);
    std::printf("  internal failure enum changed:       %zu\n", internal_diff);
    std::printf("  -> the consume mask decides WHICH option the parser\n"
                "     blames, not WHETHER the spec is rejected.  The ALLOW\n"
                "     mask decides the verdict.  So the consume mask can be\n"
                "     retuned per implementation without touching the arc's\n"
                "     contract.\n");
    std::printf("  -> the PUBLIC kind does not move, even where the INTERNAL\n"
                "     enum does: the internal values it moves between both\n"
                "     map onto the same public kind.  A caller cannot see\n"
                "     the difference; only the diagnostic text changes.\n");
    std::printf("  -> Oct 30's phase-1 mask was libc++'s, hard-wired for\n"
                "     BOTH policies.  The table makes it explicit.\n");

    c.expect_eq(total, kGridCells, "cells compared");
    c.check(rejected > 0);
    c.expect_eq(verdict_diff, 0, "verdict changes from the consume mask");
    // The PUBLIC contract is what must not move.
    c.expect_eq(kind_diff, 0, "public FormatErrorKind changes from the consume mask");
    // The internal enum and the diagnostic detail are expected to move.
    c.check(internal_diff > 0);
    c.check(detail_diff > 0);
}

// ---------------------------------------------------------------------------
// Section 6 — the arc's own paths, end to end.
// ---------------------------------------------------------------------------
void section6(counters& c) {
    section("Section 6 — the arc's own paths, end to end");

    using petra::detail::try_format_runtime_n_impl;

    char buf[64];

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:>8.3f}", 3.14159);
        c.check(r.has_value());
        if (r) {
            c.check(*r == 8);
            c.check(std::string_view(buf, *r) == "   3.142");
            std::printf("  {:>8.3f} on 3.14159 -> \"%.*s\" (%zu bytes)\n",
                        static_cast<int>(*r), buf, *r);
        }
    }

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:.2}", 42);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatArgument);
            std::printf("  {:.2} on an int -> kind=FormatArgument  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:+}", std::string_view{"x"});
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatArgument);
            std::printf("  {:+} on a string -> kind=FormatArgument  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:.}", 42);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::FormatParse);
            std::printf("  {:.} on an int -> kind=FormatParse  \"%s\"\n",
                        r.error().message.c_str());
        }
    }

    {
        const auto r = try_format_runtime_n_impl(buf, 3, "{}", 123456);
        c.check(!r.has_value());
        if (!r) {
            c.check(r.error().kind == petra::FormatErrorKind::TooLarge);
            c.check(r.error().needed_bytes == 6);
            std::printf("  cap=3 for \"123456\" -> kind=TooLarge needed=%zu\n",
                        r.error().needed_bytes);
        }
    }

    {
        char small[8] = {'X','X','X','X','X','X','X','X'};
        const auto r = try_format_runtime_n_impl(small, sizeof(small), "{:.2}", 42);
        c.check(!r.has_value());
        bool untouched = true;
        for (const char ch : small) {
            if (ch != 'X') untouched = false;
        }
        c.check(untouched);
        std::printf("  buffer atomicity on a Format error: %s\n",
                    untouched ? "preserved" : "CLOBBERED");
    }

    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{}",
                                                 petra_test::point{3, 4});
        c.check(r.has_value());
        if (r) {
            c.check(std::string_view(buf, *r) == "(3,4)");
            std::printf("  a user-defined formatter -> \"%.*s\" (validator abstained)\n",
                        static_cast<int>(*r), buf);
        }
    }
}

// ============================================================================
// Section 7 — THE NUMERIC CEILING.
//
// The standard's grammar ([format.string.std]) is
//
//     width:     positive-integer
//     precision: . nonnegative-integer
//
// and the standard says only "the value of the positive-integer is
// interpreted as a decimal integer and used as the value of the option".
// There is NO upper bound anywhere in the standard, so the ceiling is an
// IMPLEMENTATION's limit, and each implementation has its own:
//
//     libc++ 21      __format/format_string.h:  __number_max = INT32_MAX
//     libstdc++ 15   format:  __parse_integer() returns unsigned short
//
// This section MEASURES the ceiling of the linked stdlib and CHECKS it
// against the hand-written policy row -- the same discipline as Nov 7's
// derived override table.  A number in a comment is not a check; a
// number the program computes is.
// ============================================================================

// A sink that throws after `limit` bytes.
//
// A width of 2^31 would otherwise ask for a two-gigabyte string; a
// precision of 2^31 on a double would ask for two billion significant
// digits.  Both are legitimate format strings, so the only safe way to
// ask "does this spec PARSE?" is to stop the output the moment it
// starts.
struct sink_stop : std::runtime_error {
    sink_stop() : std::runtime_error("sink stop") {}
};

class aborting_sink {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;

    explicit aborting_sink(std::size_t limit) noexcept : limit_(limit) {}

    aborting_sink& operator*() noexcept { return *this; }
    aborting_sink& operator++() noexcept { return *this; }
    aborting_sink& operator++(int) noexcept { return *this; }
    aborting_sink& operator=(char) {
        if (++written_ > limit_) throw sink_stop{};
        return *this;
    }

    [[nodiscard]] std::size_t written() const noexcept { return written_; }

private:
    std::size_t written_{0};
    std::size_t limit_{0};
};

// Does this WIDTH literal parse?  Parsing happens before any output, so
// there are only two outcomes -- accepted or rejected -- and the sink
// only stops the padding.
[[nodiscard]] bool width_literal_parses(std::uint64_t w) {
    const std::string fmt = "{:" + std::to_string(w) + "}";
    const std::string s = "x";
    try {
        aborting_sink sink{1};
        (void)std::vformat_to(sink, fmt, std::make_format_args(s));
        return true;
    } catch (const sink_stop&) {
        return true;   // the spec parsed; we stopped the output
    } catch (const std::format_error&) {
        return false;  // the spec was rejected before any output
    }
}

// The same question for a PRECISION literal.  On a string the precision
// truncates, so the output stays tiny; the sink is still there because
// the question is about parsing, not about output.
[[nodiscard]] bool precision_literal_parses(std::uint64_t p) {
    const std::string fmt = "{:." + std::to_string(p) + "}";
    const std::string s = "abc";
    try {
        aborting_sink sink{1};
        (void)std::vformat_to(sink, fmt, std::make_format_args(s));
        return true;
    } catch (const sink_stop&) {
        return true;
    } catch (const std::format_error&) {
        return false;
    }
}

// The largest literal the linked stdlib accepts.  The predicate is
// monotone: a literal at or below the ceiling parses, one above it does
// not.
[[nodiscard]] std::uint64_t
largest_literal(bool (*parses)(std::uint64_t)) {
    std::uint64_t hi = std::uint64_t{1} << 40;
    if (!parses(1)) return 0;
    if (parses(hi)) return hi;   // no ceiling inside the search range
    std::uint64_t lo = 1;
    while (hi - lo > 1) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (parses(mid)) lo = mid; else hi = mid;
    }
    return lo;
}

// The WIDTH ceiling, measured inside a SAFE range.
//
// A width is a PADDING count.  Accepting a width of 2^31 means writing
// two billion padding characters, and libc++ does that in ONE memset
// before it hands a single byte to the output iterator -- so the
// throwing sink cannot stop it.  The width's acceptance side is
// therefore only measured up to a safety bound; above it, the width is
// checked on its REJECTION side, which costs nothing, because an
// over-ceiling width is rejected while the format string is parsed.
constexpr std::uint64_t kSafeWidthBound = std::uint64_t{1} << 20;  // 1 MiB

// The largest width the linked stdlib accepts, searching only up to
// `bound` so that no probe ever pads more than `bound` characters.
// Returns `bound + 1` when the boundary is above the bound.
[[nodiscard]] std::uint64_t
largest_width_within(std::uint64_t bound) {
    if (!width_literal_parses(1)) return 0;
    std::uint64_t lo = 1;
    std::uint64_t hi = bound + 1;
    if (width_literal_parses(hi)) return hi;   // no boundary in this range
    while (hi - lo > 1) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (width_literal_parses(mid)) lo = mid; else hi = mid;
    }
    return lo;
}

// The argument packs the census needs.  One pack per argument category,
// plus a two-int pack for the nested width argument.
enum class Pack { OneBool, OneChar, OneInt, OneBigInt, OneLong, OneDouble,
                   OneString, OneVoidp, TwoInts };

[[nodiscard]] CategoryTable pack_cats(Pack pack) {
    switch (pack) {
    case Pack::OneBool: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Boolean};
        return {a.data(), a.size()};
    }
    case Pack::OneChar: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Character};
        return {a.data(), a.size()};
    }
    case Pack::OneInt:
    case Pack::OneBigInt:
    case Pack::OneLong: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Integral};
        return {a.data(), a.size()};
    }
    case Pack::OneDouble: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::FloatingPoint};
        return {a.data(), a.size()};
    }
    case Pack::OneString: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::String};
        return {a.data(), a.size()};
    }
    case Pack::OneVoidp: {
        static constexpr std::array<ArgCategory, 1> a{ArgCategory::Pointer};
        return {a.data(), a.size()};
    }
    case Pack::TwoInts: {
        static constexpr std::array<ArgCategory, 2> a{ArgCategory::Integral,
                                                      ArgCategory::Integral};
        return {a.data(), a.size()};
    }
    }
    return {};
}

struct ProbeResult {
    bool accepted{false};
    std::string message;
};

// Run one format string against the LINKED stdlib, with an output sink
// that stops after one byte, so that no case can materialise a huge
// result.
[[nodiscard]] ProbeResult probe_stdlib(const std::string& fmt, Pack pack) {
    try {
        switch (pack) {
        case Pack::OneBool: {
            const bool b = true;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(b));
            break;
        }
        case Pack::OneChar: {
            const char ch = 'x';
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(ch));
            break;
        }
        case Pack::OneInt: {
            const int i = 7;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(i));
            break;
        }
        case Pack::OneBigInt: {
            const int i = 300;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(i));
            break;
        }
        case Pack::OneLong: {
            const long l = 7;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(l));
            break;
        }
        case Pack::OneDouble: {
            const double d = 1.5;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(d));
            break;
        }
        case Pack::OneString: {
            const std::string s = "abc";
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(s));
            break;
        }
        case Pack::OneVoidp: {
            const void* p = nullptr;
            (void)std::vformat_to(aborting_sink{1}, fmt, std::make_format_args(p));
            break;
        }
        case Pack::TwoInts: {
            const int a = 7;
            const int b = 3;
            (void)std::vformat_to(aborting_sink{1}, fmt,
                                  std::make_format_args(a, b));
            break;
        }
        }
        return {true, {}};
    } catch (const sink_stop&) {
        return {true, {}};
    } catch (const std::format_error& e) {
        return {false, e.what()};
    }
}

[[nodiscard]] const char* kind_name(FormatErrorKind k) {
    switch (k) {
    case FormatErrorKind::Format:         return "Format";
    case FormatErrorKind::FormatParse:    return "FormatParse";
    case FormatErrorKind::FormatArgument: return "FormatArgument";
    case FormatErrorKind::TooLarge:       return "TooLarge";
    }
    return "?";
}

[[nodiscard]] const char* scan_name(ScanFailure f) {
    switch (f) {
    case ScanFailure::None:               return "None";
    case ScanFailure::Malformed:          return "Malformed";
    case ScanFailure::TypeNotAllowed:     return "TypeNotAllowed";
    case ScanFailure::OptionNotAllowed:   return "OptionNotAllowed";
    case ScanFailure::IndexOutOfRange:    return "IndexOutOfRange";
    case ScanFailure::WidthArgNotInteger: return "WidthArgNotInteger";
    case ScanFailure::MixedNumbering:     return "MixedNumbering";
    case ScanFailure::NumberTooLarge:     return "NumberTooLarge";
    }
    return "?";
}

[[nodiscard]] const char* verdict_name(MessageVerdict v) {
    switch (v) {
    case MessageVerdict::Parse:     return "Parse";
    case MessageVerdict::Argument:  return "Argument";
    case MessageVerdict::Ambiguous: return "Ambiguous";
    case MessageVerdict::Unknown:   return "Unknown";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Section 7 — the ceiling, measured.
// ---------------------------------------------------------------------------
void section7(counters& c) {
    section("Section 7 — the numeric ceiling: measured, then checked");

    const ImplPolicy& linked = linked_policy();

    // The PRECISION is the safe way to measure the number parser: a
    // precision TRUNCATES, so a literal of 2^31 costs three bytes.
    const std::uint64_t measured_precision = largest_literal(&precision_literal_parses);

    // The WIDTH pads, so it is only measured inside the safety bound.
    const std::uint64_t measured_width = largest_width_within(kSafeWidthBound);

    std::printf("  linked stdlib: %s\n", linked.name);
    std::printf("  the standard's grammar fixes NO ceiling:\n");
    std::printf("    width:     positive-integer\n");
    std::printf("    precision: . nonnegative-integer\n");
    std::printf("  MEASURED on the linked stdlib:\n");
    std::printf("    largest precision literal accepted: %llu  (exact)\n",
                static_cast<unsigned long long>(measured_precision));
    std::printf("    largest width literal accepted:     %llu  (searched up to %llu)\n",
                static_cast<unsigned long long>(measured_width),
                static_cast<unsigned long long>(kSafeWidthBound));
    std::printf("  the policy row says: %s\n", linked.ceiling_note);

    // The policy row is checked against the EXACT measurement.
    if (linked.max_number.has_value()) {
        c.expect_eq(measured_precision, *linked.max_number,
                    "the measured ceiling vs the hand-written policy row");
    } else {
        c.check(std::strcmp(linked.name, "standard") == 0);
    }

    // The WIDTH is subject to the same limit, and that is checked on its
    // rejection side: a width over the ceiling is rejected while the
    // format string is parsed, which costs nothing -- no padding is
    // ever materialised.
    const std::uint64_t over = measured_precision + 1;
    c.check(!width_literal_parses(over));
    c.check(measured_width <= measured_precision);
    std::printf("  a width of %llu is rejected by the linked stdlib: %s\n",
                static_cast<unsigned long long>(over),
                width_literal_parses(over) ? "NO" : "yes");
    std::printf("  -> the width and the precision come out of the SAME number\n"
                "     parser, so they share the ceiling; the source says so,\n"
                "     the measurement pins the number, and the width's\n"
                "     rejection side confirms that the width is bound by it.\n");

    // The ARGUMENT INDEX.  The standard's grammar has no ceiling for it
    // either, and both implementations parse it with the same number
    // parser as the width, so the arc applies the same number to it.
    //
    // What the STDLIB says about an index over the ceiling is worth
    // printing, because the two implementations cannot say the same
    // thing about it: libc++ has a separate message for "too large to
    // be a number" and for "past the end of the argument list", and
    // libstdc++ has ONE message for both.
    const ProbeResult at_probe =
        probe_stdlib("{" + std::to_string(measured_precision) + "}", Pack::OneInt);
    const ProbeResult over_probe =
        probe_stdlib("{" + std::to_string(over) + "}", Pack::OneInt);

    std::printf("\n  the argument index, which uses the same number parser:\n");
    std::printf("    index %llu (at the ceiling):    %s\n",
                static_cast<unsigned long long>(measured_precision),
                at_probe.accepted ? "accepted" : at_probe.message.c_str());
    std::printf("      the arc's message table calls that: %s\n",
                verdict_name(verdict_from_message(at_probe.message.c_str())));
    std::printf("    index %llu (over the ceiling):   %s\n",
                static_cast<unsigned long long>(over),
                over_probe.accepted ? "accepted" : over_probe.message.c_str());
    std::printf("      the arc's message table calls that: %s\n",
                verdict_name(verdict_from_message(over_probe.message.c_str())));

    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        // libc++ distinguishes the two failures by message.
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(at_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Argument),
                    "libc++: an index at the ceiling is an argument failure");
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(over_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Parse),
                    "libc++: an index over the ceiling is a parse failure");
    } else {
        // libstdc++ uses ONE message for both, so the MESSAGE cannot
        // tell them apart -- only the scanner's own rule can.  That is
        // the whole argument for putting the ceiling in the policy.
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(at_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Unknown),
                    "libstdc++: an index at the ceiling is unrecognised");
        c.expect_eq(static_cast<std::size_t>(verdict_from_message(over_probe.message)),
                    static_cast<std::size_t>(MessageVerdict::Unknown),
                    "libstdc++: an index over the ceiling is unrecognised too");
    }
}

// ---------------------------------------------------------------------------
// Section 8 — THE CENSUS.
//
// Every case where the arc and the stdlib can disagree, with THREE
// classifications side by side:
//
//   today      the scanner without the ceiling rule, and the fallback
//              that DISCARDS the scanner's verdict (Nov 7's behaviour)
//   +ceiling   the scanner WITH the ceiling rule, same discarding
//              fallback
//   +carried   the ceiling rule AND the fallback that CARRIES the
//              scanner's verdict
//
// The middle column is the point: the ceiling rule and the carried
// verdict are two different repairs, and the lesson has to know which
// one moved which cell.
// ---------------------------------------------------------------------------
void section8(counters& c) {
    section("Section 8 — the census: what the carried verdict actually changes");

    const ImplPolicy& linked = linked_policy();

    // "Today" is the same implementation with the ceiling rule switched
    // OFF -- which is exactly what a policy without a ceiling means.
    const ImplPolicy today{
        linked.name, linked.allow_rows, linked.allow_count,
        linked.consume_rows, linked.consume_count,
        std::nullopt, linked.ceiling_note, linked.fill};

    // The ceiling is measured through the PRECISION: a precision
    // truncates, so the measurement never materialises a gigabyte of
    // padding the way a width would.
    const std::uint64_t ceiling = largest_literal(&precision_literal_parses);
    const std::string at = std::to_string(ceiling);
    const std::string big = std::to_string(ceiling + 1);

    struct Case {
        const char* label;
        std::string fmt;
        Pack pack;
    };

    const std::vector<Case> cases = {
        {"width literal at the safe bound",    "{:1048576}",         Pack::OneString},
        {"width literal over the ceiling",      "{:" + big + "}",     Pack::OneString},
        {"precision literal at the ceiling",    "{:." + at + "}",     Pack::OneString},
        {"precision literal over the ceiling",  "{:." + big + "}",   Pack::OneString},
        {"argument index at the ceiling",       "{" + at + "}",       Pack::OneInt},
        {"argument index over the ceiling",     "{" + big + "}",      Pack::OneInt},
        {"nested index over the ceiling",       "{0:{" + big + "}}",  Pack::TwoInts},
        {"argument index past the end",         "{1}",                Pack::OneInt},
        {"value out of range for 'c'",          "{:c}",               Pack::OneBigInt},
        {"invalid UTF-8 fill",                  "{:\x80<5}",          Pack::OneString},
        {"a multi-byte UTF-8 fill",             "{:\xF0\x9F\xA4\x91^5}", Pack::OneString},
        {"fill is '{'",                          "{:{<5}",             Pack::OneString},
        {"precision on an integer",             "{:.2}",              Pack::OneInt},
    };

    std::size_t rejects = 0;
    std::size_t changed_by_ceiling = 0;
    std::size_t changed_by_carry = 0;
    std::size_t changed_total = 0;
    std::size_t false_positives = 0;

    std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", "case", "stdlib",
                "scanner (today)", "today", "+ceiling", "+carried");

    for (const Case& cs : cases) {
        const ProbeResult pr = probe_stdlib(cs.fmt, cs.pack);
        const CategoryTable cats = pack_cats(cs.pack);
        const ScanResult s_today = scan_format_string(cs.fmt, cats, today);

        if (!pr.accepted) {
            ++rejects;
            const Classification a = classify_format_failure(
                cs.fmt, pr.message.c_str(), cats, today, Fallback::Discard);
            const Classification b = classify_format_failure(
                cs.fmt, pr.message.c_str(), cats, linked, Fallback::Discard);
            const Classification cc = classify_format_failure(
                cs.fmt, pr.message.c_str(), cats, linked, Fallback::Carry);
            if (a.kind != b.kind) ++changed_by_ceiling;
            if (b.kind != cc.kind) ++changed_by_carry;
            if (a.kind != cc.kind) ++changed_total;
            std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", cs.label,
                        "reject", scan_name(s_today.failure), kind_name(a.kind),
                        kind_name(b.kind), kind_name(cc.kind));
        } else {
            // The stdlib accepts it.  Does the arc's scanner?
            const ScanResult s_new = scan_format_string(cs.fmt, cats, linked);
            if (!s_new.ok()) {
                ++false_positives;
                std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", cs.label,
                            "ACCEPT", scan_name(s_new.failure), "FALSE POSITIVE",
                            "FALSE POSITIVE", "FALSE POSITIVE");
            } else {
                std::printf("  %-35s %-7s %-17s %-13s %-13s %-13s\n", cs.label,
                            "accept", "None", "-", "-", "-");
            }
        }
    }

    // The chain item's own question, asked of the 1,596-cell grid first:
    // how many of the rejects would classify differently?
    //
    // The answer is zero, and the reason is the finding: the grid never
    // reaches the fallback.  Every reject in the grid is decided by the
    // scanner, so the grid is structurally blind to this defect.
    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    std::size_t grid_rejects = 0;
    std::size_t grid_reached_fallback = 0;
    std::size_t grid_changed = 0;

    const auto run = [&](ArgCategory cat, auto& value) {
        const ArgCategory one[] = {cat};
        const CategoryTable table{one, 1};
        for (std::size_t p = 0; p < kPrefixCount; ++p) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                const std::string fmt = "{:" + spec_text(p, t) + "}";
                std::string message;
                const Outcome outcome = measure(fmt, value, message);
                if (outcome == Outcome::Ok) continue;
                ++grid_rejects;
                const Classification a = classify_format_failure(
                    fmt, message.c_str(), table, today, Fallback::Discard);
                const Classification cc = classify_format_failure(
                    fmt, message.c_str(), table, linked, Fallback::Carry);
                if (a.used_message_table) ++grid_reached_fallback;
                if (a.kind != cc.kind) ++grid_changed;
            }
        }
    };

    run(ArgCategory::Boolean,       b);
    run(ArgCategory::Character,     ch);
    run(ArgCategory::Integral,      i);
    run(ArgCategory::Integral,      l);
    run(ArgCategory::FloatingPoint, d);
    run(ArgCategory::String,        sv);
    run(ArgCategory::Pointer,       vp);

    std::printf("\n  the 1,596-cell grid: %zu rejects, %zu of them reach the\n"
                "  fallback, %zu change their public kind.\n",
                grid_rejects, grid_reached_fallback, grid_changed);
    std::printf("  -> the grid CANNOT see this defect.  Every one of its\n"
                "     rejects is decided by the scanner, so the fallback\n"
                "     never runs, and a change to the fallback cannot move\n"
                "     a single cell.  Measuring the defect needs the\n"
                "     families the grid omits.\n");

    std::printf("\n  the census: %zu rejects; the ceiling rule moved %zu, the\n"
                "  carried verdict moved %zu; %zu in total.\n",
                rejects, changed_by_ceiling, changed_by_carry, changed_total);
    std::printf("  the arc's scanner rejects what the stdlib accepts in %zu case(s).\n",
                false_positives);
    std::printf("  -> the ceiling rule is what makes \"even when the message is\n"
                "     unrecognised\" true: the scanner decides the magnitude\n"
                "     family with no message at all.  The carried verdict is\n"
                "     what rescues the families the scanner cannot model --\n"
                "     and only where the stdlib's message vocabulary lets it.\n");
    std::printf("  -> and the FILL family no longer needs it at all: the\n"
                "     scanner now reads the fill as a Unicode scalar value,\n"
                "     so the ill-formed fill is a STRUCTURAL failure.  That is\n"
                "     why the carried verdict moves nothing here.\n");

    c.expect_eq(grid_changed, 0, "the grid does not move");
    c.expect_eq(grid_reached_fallback, 0, "the grid never reaches the fallback");
    c.check(rejects > 0);
    c.check(changed_total > 0);
    c.check(changed_by_ceiling > 0);
}

// ---------------------------------------------------------------------------
// Section 9 — THE FILL AS A CHARACTER.
//
// Nov 12's next-step item (1), asked three things: decode the fill as
// a UTF-8 scalar value, validate it, and re-measure the
// false-positive count.  This section does all three, and it measures
// the OLD rule and the NEW rule side by side -- because the point is
// not that the new rule is nicer, it is that the new rule AGREES with
// the stdlib where the old one did not.
// ---------------------------------------------------------------------------
void section9(counters& c) {
    section("Section 9 — the fill as a character");

    const ImplPolicy& linked = linked_policy();

    // ---- 9.1 the decoder, on its own ------------------------------------
    struct DecodeCase {
        const char* label;
        const char* bytes;
        std::size_t in_len;   // how many bytes to feed the decoder
        FillFault fault;
        std::size_t out_len;  // how many bytes it consumes
        char32_t value;
    };
    const DecodeCase decode_cases[] = {
        {"ASCII 'A'",              "A",                 1, FillFault::None, 1, U'A'},
        {"U+00E9 e-acute",         "\xC3\xA9",          2, FillFault::None, 2, 0x00E9u},
        {"U+20AC euro",            "\xE2\x82\xAC",      3, FillFault::None, 3, 0x20ACu},
        {"U+1F911 money-with-wings","\xF0\x9F\xA4\x91",  4, FillFault::None, 4, 0x1F911u},
        {"U+10FFFF, the last one", "\xF4\x8F\xBF\xBF",  4, FillFault::None, 4, 0x10FFFFu},
        {"a lone continuation",    "\x80",              1, FillFault::InvalidLead, 1, 0},
        {"0xFE, never a lead",     "\xFE",              1, FillFault::InvalidLead, 1, 0},
        {"0xC0, overlong-only lead","\xC0",              1, FillFault::Truncated, 1, 0},
        {"0xF5, wants a 4th byte", "\xF5",              1, FillFault::Truncated, 1, 0},
        {"a truncated 3-byte",     "\xE2\x82",          2, FillFault::Truncated, 2, 0},
        {"a truncated 4-byte",     "\xF0\x9F\xA4",      3, FillFault::Truncated, 3, 0},
        {"a byte that cannot continue","\xE2\x28",       2, FillFault::Truncated, 1, 0},
        {"an overlong 2-byte",     "\xC0\xAF",          2, FillFault::Overlong, 1, 0},
        {"an overlong 3-byte",     "\xE0\x80",          2, FillFault::Overlong, 1, 0},
        {"an overlong 4-byte",     "\xF0\x80",          2, FillFault::Overlong, 1, 0},
        {"a surrogate U+D800",     "\xED\xA0",          2, FillFault::Surrogate, 1, 0},
        {"above U+10FFFF",         "\xF4\x90",          2, FillFault::OutOfRange, 1, 0},
    };

    std::printf("  the decoder -- Unicode 15 Table 3-7, read backwards:\n");
    std::printf("    %-27s %-6s %-16s %s\n", "bytes", "len", "fault", "value");
    for (const DecodeCase& dc : decode_cases) {
        const Utf8CodePoint cp =
            decode_utf8(std::string_view{dc.bytes, dc.in_len});
        c.expect_eq(static_cast<std::size_t>(cp.fault),
                    static_cast<std::size_t>(dc.fault),
                    "the decoder's fault");
        c.expect_eq(cp.length, dc.out_len, "the decoder's length");
        if (dc.fault == FillFault::None) {
            c.expect_eq(static_cast<std::size_t>(cp.value),
                        static_cast<std::size_t>(dc.value),
                        "the decoder's value");
        }
        std::printf("    %-27s %-6zu %-16s U+%04lX\n", dc.label, cp.length,
                    fault_name(cp.fault),
                    static_cast<unsigned long>(cp.value));
    }

    // ---- 9.2 the fill census: the OLD rule vs the NEW rule --------------
    //
    // Every case where the fill can be ill-formed or multi-byte, with
    // THREE verdicts side by side: the linked stdlib, the arc's OLD
    // byte-oriented scanner, and the arc's NEW scalar-value scanner.
    struct FillCase {
        const char* label;
        const char* fmt;
    };
    const FillCase fill_cases[] = {
        {"an ASCII fill",            "{:*<5}"},
        {"a 2-byte fill (U+00E9)",   "{:\xC3\xA9<5}"},
        {"a 3-byte fill (U+20AC)",   "{:\xE2\x82\xAC<5}"},
        {"a 4-byte fill (U+1F911)",  "{:\xF0\x9F\xA4\x91^5}"},
        {"U+10FFFF as a fill",       "{:\xF4\x8F\xBF\xBF<5}"},
        {"a lone continuation byte", "{:\x80<5}"},
        {"0xFE as a fill",           "{:\xFE<5}"},
        {"0xC0 as a fill",           "{:\xC0<5}"},
        {"0xF5 as a fill",           "{:\xF5<5}"},
        {"a truncated 3-byte fill",  "{:\xE2\x82<5}"},
        {"a truncated 4-byte fill",  "{:\xF0\x9F\xA4<5}"},
        {"a bad continuation byte",  "{:\xE2\x28\xA1<5}"},
        {"an overlong 2-byte fill",  "{:\xC0\xAF<5}"},
        {"an overlong 3-byte fill",  "{:\xE0\x80\x80<5}"},
        {"an overlong 4-byte fill",  "{:\xF0\x80\x80\x80<5}"},
        {"a surrogate fill",         "{:\xED\xA0\x80<5}"},
        {"a value above U+10FFFF",   "{:\xF4\x90\x80\x80<5}"},
    };

    // The OLD rule: the same scanner with the fill read as one byte.
    ImplPolicy byte_rule = linked;
    byte_rule.fill.unicode = false;

    const CategoryTable cats = pack_cats(Pack::OneString);

    std::size_t old_false_positives = 0;
    std::size_t old_false_negatives = 0;
    std::size_t new_false_positives = 0;
    std::size_t new_false_negatives = 0;

    std::printf("\n  the fill census -- the stdlib, the OLD byte rule, the NEW"
                " scalar rule:\n");
    std::printf("    %-27s %-8s %-14s %-14s\n", "case", "stdlib",
                "byte rule", "scalar rule");

    for (const FillCase& fc : fill_cases) {
        const ProbeResult pr = probe_stdlib(fc.fmt, Pack::OneString);
        const ScanResult s_old = scan_format_string(fc.fmt, cats, byte_rule);
        const ScanResult s_new = scan_format_string(fc.fmt, cats, linked);

        const bool stdlib_ok = pr.accepted;
        const bool old_ok = s_old.ok();
        const bool new_ok = s_new.ok();

        if (!old_ok && stdlib_ok) ++old_false_positives;
        if (old_ok && !stdlib_ok) ++old_false_negatives;
        if (!new_ok && stdlib_ok) ++new_false_positives;
        if (new_ok && !stdlib_ok) ++new_false_negatives;

        const auto verdict = [](bool ok) -> const char* {
            return ok ? "accept" : "reject";
        };
        std::printf("    %-27s %-8s %-14s %-14s\n", fc.label,
                    verdict(stdlib_ok), verdict(old_ok), verdict(new_ok));
    }

    std::printf("\n  the byte-oriented rule disagrees with the linked stdlib"
                " %zu time(s):\n"
                "    %zu false positive(s) (the arc rejects, the stdlib accepts)\n"
                "    %zu false negative(s) (the arc accepts, the stdlib rejects)\n",
                old_false_positives + old_false_negatives, old_false_positives,
                old_false_negatives);
    std::printf("  the scalar-value rule disagrees with the linked stdlib"
                " %zu time(s):\n"
                "    %zu false positive(s), %zu false negative(s)\n",
                new_false_positives + new_false_negatives, new_false_positives,
                new_false_negatives);
    std::printf("  -> the fill is a scalar value, so the scanner DECODES it.\n"
                "     The old rule guessed a byte and asked whether the NEXT\n"
                "     BYTE was an alignment option; a four-byte fill has no\n"
                "     such byte, so the arc called a valid spec malformed.\n");

    c.expect_eq(new_false_positives, 0, "the scalar rule has no false positives");
    c.expect_eq(new_false_negatives, 0, "the scalar rule has no false negatives");
    c.check(old_false_positives + old_false_negatives > 0);

    // ---- 9.3 the ill-formed fill is a STRUCTURAL rule now ---------------
    //
    // Under libc++ this used to be rescued by the message table: the
    // stdlib said "The format specifier contains malformed Unicode
    // characters", that message was in the arc's libc++-shaped table,
    // and the carried verdict carried the parse verdict through.  Now
    // the scanner rejects the fill on its own, so the message table is
    // not consulted at all.
    {
        const std::string fmt = "{:\x80<5}";
        const ProbeResult pr = probe_stdlib(fmt, Pack::OneString);
        std::printf("\n  an ill-formed fill, decided by the scanner:\n");
        if (pr.accepted) {
            std::printf("    the linked stdlib ACCEPTS it (it substitutes"
                        " U+FFFD)\n");
            const ScanResult s = scan_format_string(fmt, cats, linked);
            c.check(s.ok());
        } else {
            std::printf("    the linked stdlib rejects it: %s\n",
                        pr.message.c_str());
            const Classification cl = classify_format_failure(
                fmt, pr.message.c_str(), cats, linked, Fallback::Carry);
            c.check(!cl.used_message_table);
            c.check(cl.scanner_verdict == ScanFailure::Malformed);
            c.expect_eq(static_cast<std::size_t>(cl.kind),
                        static_cast<std::size_t>(FormatErrorKind::FormatParse),
                        "an ill-formed fill is a FormatParse");
            std::printf("    the scanner says: %s\n", cl.reason);
            std::printf("    the message table was consulted: %s\n",
                        cl.used_message_table ? "yes" : "NO");
        }
    }

    // ---- 9.4 the width counts CODE POINTS, not bytes --------------------
    //
    // A width of 10 with a four-byte fill produces 31 bytes: ten code
    // points, seven of which are the fill.  The arc's cap is in BYTES
    // -- it is a buffer cap, measured from the stdlib's own output --
    // so the arc is right, and the two units must not be confused.
    {
        const auto format_bytes = [](std::string_view fmt, std::string& arg) {
            std::string out;
            std::vformat_to(std::back_inserter(out), fmt,
                             std::make_format_args(arg));
            return out;
        };

        std::string three_euros = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
        const std::string out5 = format_bytes("{:*^5}", three_euros);
        std::printf("\n  the width counts code points, not bytes:\n");
        std::printf("    \"{:*^5}\" with 3 euros -> %zu bytes, %zu code points\n",
                    out5.size(), count_code_points(out5));
        c.expect_eq(count_code_points(out5), 5,
                    "a width of 5 is five code points");
        c.expect_eq(out5.size(), 11, "three 3-byte euros plus two fill bytes");

        std::string abc = "abc";
        const std::string out10 = format_bytes("{:\xF0\x9F\xA4\x91^10}", abc);
        std::printf("    \"{:\xF0\x9F\xA4\x91^10}\" with \"abc\" -> %zu bytes,"
                    " %zu code points\n",
                    out10.size(), count_code_points(out10));
        c.expect_eq(count_code_points(out10), 10,
                    "a width of 10 is ten code points");
        c.expect_eq(out10.size(), 31, "seven four-byte fills plus three bytes");
    }
}

// ---------------------------------------------------------------------------
// Section 10 — the arc's own paths, extended.
// ---------------------------------------------------------------------------
void section10(counters& c) {
    section("Section 10 — the arc's own paths, extended");

    using petra::detail::try_format_runtime_n_impl;

    const bool libcxx = std::strcmp(linked_policy().name, "libc++ 21") == 0;
    // The ceiling is measured through the PRECISION: a precision
    // truncates, so the measurement never materialises a gigabyte of
    // padding the way a width would.
    const std::uint64_t ceiling = largest_literal(&precision_literal_parses);
    const std::string over = std::to_string(ceiling + 1);

    char buf[64];

    // A width over the ceiling: a malformed FORMAT STRING, in both
    // builds, whichever message the stdlib happens to use.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf),
                                                 "{:" + over + "}", 42);
        c.check(!r.has_value());
        if (!r) {
            std::printf("  a width over the ceiling      -> %s\n",
                        kind_name(r.error().kind));
            c.expect_eq(static_cast<std::size_t>(r.error().kind),
                        static_cast<std::size_t>(FormatErrorKind::FormatParse),
                        "a width over the ceiling is a FormatParse");
        }
    }

    // An argument index over the ceiling: also a malformed format
    // string, and this one used to be an ARGUMENT failure.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf),
                                                 "{" + over + "}", 42);
        c.check(!r.has_value());
        if (!r) {
            std::printf("  an index over the ceiling      -> %s\n",
                        kind_name(r.error().kind));
            c.expect_eq(static_cast<std::size_t>(r.error().kind),
                        static_cast<std::size_t>(FormatErrorKind::FormatParse),
                        "an index over the ceiling is a FormatParse");
        }
    }

    // A value that does not fit the presentation type.  This one is the
    // arc's known gap: the scanner cannot see VALUES, and libstdc++'s
    // message is not in the arc's table.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf), "{:c}", 300);
        c.check(!r.has_value());
        if (!r) {
            std::printf("  a value out of range for 'c'   -> %s\n",
                        kind_name(r.error().kind));
            if (libcxx) {
                c.expect_eq(static_cast<std::size_t>(r.error().kind),
                            static_cast<std::size_t>(FormatErrorKind::FormatArgument),
                            "libc++: a value out of range is a FormatArgument");
            } else {
                c.expect_eq(static_cast<std::size_t>(r.error().kind),
                            static_cast<std::size_t>(FormatErrorKind::Format),
                            "libstdc++: a value out of range is still generic");
            }
        }
    }

    // An invalid UTF-8 fill.  libc++ rejects it (the fill must be a
    // single Unicode scalar value) and the carried verdict keeps it a
    // parse failure; libstdc++ accepts the byte.
    {
        const auto r = try_format_runtime_n_impl(buf, sizeof(buf),
                                                 "{:\x80<5}", std::string{"abc"});
        if (libcxx) {
            c.check(!r.has_value());
            if (!r) {
                std::printf("  an invalid UTF-8 fill        -> %s\n",
                            kind_name(r.error().kind));
                c.expect_eq(static_cast<std::size_t>(r.error().kind),
                            static_cast<std::size_t>(FormatErrorKind::FormatParse),
                            "libc++: an invalid UTF-8 fill is a FormatParse");
            }
        } else {
            c.check(r.has_value());
            std::printf("  an invalid UTF-8 fill        -> accepted by libstdc++ 15\n");
        }
    }
}

// ============================================================================
// Section 11 — THE COMBINATION GRID.
//
// The 1,596-cell grid of Section 3 isolates ONE option per cell, so a
// rule that only exists where TWO options MEET is invisible to it: no
// single-option cell has an align option next to a '0' option.
//
// This grid puts every PAIR of the seven option slots in one cell:
//
//     21 pairs x 19 type chars x 7 rows = 2,793 cells
//
// and it measures the model against the LINKED stdlib twice: with
// today's rule (the '0' is ignored when an align option is present)
// and with the OLD rule (the '0' is always validity-checked).
// ============================================================================

// Which phase of the model decided a spec.
enum class ModelPhase { Accept, Consume, Allow, Type, Structural };

[[nodiscard]] const char* model_phase_name(ModelPhase p) {
    switch (p) {
    case ModelPhase::Accept:     return "accept";
    case ModelPhase::Consume:    return "phase 1 (the consume mask)";
    case ModelPhase::Allow:      return "phase 2 (the allow mask)";
    case ModelPhase::Type:       return "phase 3 (the presentation type)";
    case ModelPhase::Structural: return "structural";
    }
    return "?";
}

// The seven option slots, in the STANDARD's grammar order, so that
// every PAIR of them is a spec the grammar can produce.
constexpr std::size_t kSlotCount = 7;
constexpr std::size_t kComboCells = 21u * 19u * 7u;

constexpr const char* kSlotText[kSlotCount] = {
    "<", "+", "#", "0", "10", ".2", "L",
};

// ---------------------------------------------------------------------------
// Section 11 — the combination grid, measured against the LINKED stdlib.
// ---------------------------------------------------------------------------
void section11(counters& c) {
    section("Section 11 — the COMBINATION grid: 21 option PAIRS x 19 types x 7 rows");

    const ImplPolicy& linked = linked_policy();

    // The OLD rule: the '0' option is validity-checked even when an
    // align option is present.  This is the arc's rule until today.
    ImplPolicy old_rule = linked;
    old_rule.zero_ignored_with_align = false;

    // The model's own verdict, and WHICH phase decided it.
    const auto phase_of = [&](ArgCategory cat, std::string_view spec,
                              const ImplPolicy& pol) -> ModelPhase {
        SpecFailure sf = SpecFailure::None;
        Opt bad = Opt::None;
        const ParsedSpec parsed =
            parse_spec(spec, policy_consume_mask(cat, pol), sf, bad,
                       pol.max_number, pol.fill, pol.zero_ignored_with_align);
        if (sf != SpecFailure::None) {
            return (sf == SpecFailure::OptionNotAllowed) ? ModelPhase::Consume
                                                         : ModelPhase::Structural;
        }
        const SpecVerdict v =
            validate_spec(cat, parsed,
                          policy_allow_mask(cat, parsed.type_char, pol));
        if (v.failure == SpecFailure::TypeNotAllowed) return ModelPhase::Type;
        if (v.failure == SpecFailure::OptionNotAllowed) return ModelPhase::Allow;
        return ModelPhase::Accept;
    };

    std::size_t total = 0;
    std::size_t agree_new = 0;
    std::size_t agree_old = 0;
    std::size_t phase_accept = 0;
    std::size_t phase_consume = 0;
    std::size_t phase_allow = 0;
    std::size_t phase_type = 0;
    std::size_t phase_structural = 0;
    std::vector<std::string> divergent;

    const auto run = [&](const char* label, ArgCategory cat, auto& value) {
        for (std::size_t i = 0; i < kSlotCount; ++i) {
            for (std::size_t j = i + 1; j < kSlotCount; ++j) {
                const std::string pair = std::string{kSlotText[i]} + kSlotText[j];
                for (std::size_t t = 0; t < kTypeCount; ++t) {
                    const std::string spec = pair + kTypeChars[t].text;
                    const std::string fmt = "{:" + spec + "}";
                    std::string message;
                    const Outcome outcome = measure(fmt, value, message);
                    const bool stdlib_ok = (outcome == Outcome::Ok);

                    std::string reason_new;
                    std::string reason_old;
                    const bool model_new =
                        model_verdict(cat, spec, linked, reason_new);
                    const bool model_old =
                        model_verdict(cat, spec, old_rule, reason_old);

                    ++total;
                    if (model_new == stdlib_ok) ++agree_new;
                    if (model_old == stdlib_ok) ++agree_old;

                    switch (phase_of(cat, spec, linked)) {
                    case ModelPhase::Accept:     ++phase_accept; break;
                    case ModelPhase::Consume:    ++phase_consume; break;
                    case ModelPhase::Allow:      ++phase_allow; break;
                    case ModelPhase::Type:       ++phase_type; break;
                    case ModelPhase::Structural: ++phase_structural; break;
                    }

                    if (model_old != stdlib_ok && divergent.size() < 16) {
                        divergent.push_back(
                            std::string{"    "} + label + " | {:" + spec + "} | "
                            + "the OLD rule says "
                            + (model_old ? "ACCEPT" : "REJECT")
                            + ", today's rule says "
                            + (model_new ? "ACCEPT" : "REJECT")
                            + ", the stdlib says "
                            + (stdlib_ok ? "ACCEPT" : "REJECT"));
                    }
                }
            }
        }
    };

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    run("bool",   ArgCategory::Boolean,       b);
    run("char",   ArgCategory::Character,     ch);
    run("int",    ArgCategory::Integral,      i);
    run("long",   ArgCategory::Integral,      l);
    run("double", ArgCategory::FloatingPoint, d);
    run("string", ArgCategory::String,        sv);
    run("voidp",  ArgCategory::Pointer,       vp);

    std::printf("  the grid: 21 option PAIRS x 19 type chars x 7 rows"
                " = %zu cells\n", total);
    std::printf("  the linked stdlib is \"%s\"\n", linked.name);
    std::printf("  the model with TODAY's rule   vs the stdlib: %zu/%zu agree\n",
                agree_new, total);
    std::printf("  the model with the OLD rule    vs the stdlib: %zu/%zu agree\n",
                agree_old, total);

    std::printf("\n  the model's phases over the %zu cells:\n", total);
    std::printf("    accept %zu   phase 1 (consume) %zu   phase 2 (allow) %zu\n",
                phase_accept, phase_consume, phase_allow);
    std::printf("    phase 3 (type) %zu   structural %zu\n",
                phase_type, phase_structural);

    if (!divergent.empty()) {
        std::printf("\n  the cells where the OLD rule disagrees with the stdlib:\n");
        for (const std::string& div : divergent) {
            std::printf("%s\n", div.c_str());
        }
        std::printf("    -> every one of them is the align option next to the\n"
                    "       '0' option.  The single-option grid has no such\n"
                    "       cell, so it could not have seen this.\n");
    }

    // The chain item's own question: does "{:+c}" on a char fail at
    // phase 1 (the sign is not in the consume mask) or phase 2 (the
    // narrowing)?
    {
        const ModelPhase ph = phase_of(ArgCategory::Character, "+c", linked);
        std::string message;
        const Outcome outcome = measure("{:+c}", ch, message);
        std::printf("\n  the chain's own question: \"{:+c}\" on a char\n");
        std::printf("    the model's phase: %s\n", model_phase_name(ph));
        std::printf("    the linked stdlib: %s%s%s\n",
                    outcome == Outcome::Ok ? "ACCEPTS" : "REJECTS",
                    outcome == Outcome::Ok ? "" : ": ",
                    outcome == Outcome::Ok ? "" : message.c_str());
        std::printf("    -> phase 2.  The sign IS in charT's consume mask, so\n"
                    "       the parser reads it; what rejects the spec is the\n"
                    "       ALLOW mask, because 'c' is not an integer\n"
                    "       presentation type for charT.\n");
        c.expect_eq(static_cast<std::size_t>(ph),
                    static_cast<std::size_t>(ModelPhase::Allow),
                    "the chain question: phase 2, the allow mask");
        c.check(outcome != Outcome::Ok);
    }

    c.expect_eq(total, kComboCells, "the combination grid's cell count");
    c.expect_eq(total - agree_new, 0,
                "the model (today's rule) agrees with the linked stdlib");
    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        c.expect_eq(total - agree_old, 7,
                    "the OLD rule's disagreements under libc++ 21");
    } else {
        c.expect_eq(total - agree_old, 0,
                    "the OLD rule's disagreements under libstdc++ 15");
    }
}

// ============================================================================
// Section 12 — THE TRIPLE GRID.
//
// Nov 14's grid measured PAIRS of the seven option slots: 21 pairs x 19
// type chars x 7 rows = 2,793 cells.  A rule that only exists where TWO
// options MEET is invisible to a single-option grid, and a rule that only
// exists where THREE options meet is invisible to a PAIR grid.  So Nov
// 14's silence was evidence, not proof.
//
// This grid measures TRIPLES:
//
//     C(7,3) = 35 triples x 19 type chars x 7 rows = 4,655 cells
//
// and asks the chain's question: is the pair grid's answer still the
// answer, or does some rule need all three options at once?
// ============================================================================

// The two option slots whose INTERACTION Nov 14 found.  Their indices in
// kSlotText are fixed by the standard's grammar order, so "the triple
// contains both" is a statement about the grammar and not about a
// particular spec.
constexpr std::size_t kAlignSlot = 0u;   // "<"
constexpr std::size_t kZeroSlot  = 3u;   // "0"

static_assert(kSlotText[kAlignSlot][0] == '<' && kSlotText[kAlignSlot][1] == '\0',
              "slot 0 is the align option");
static_assert(kSlotText[kZeroSlot][0] == '0' && kSlotText[kZeroSlot][1] == '\0',
              "slot 3 is the zero-padding option");

constexpr std::size_t kTripleCount = 35u;      // C(7,3)
constexpr std::size_t kTripleCells = kTripleCount * kTypeCount * kRowCount;

static_assert(kTripleCount == 35u, "C(7,3) triples of the seven option slots");
static_assert(kTripleCells == 4655u, "35 triples x 19 type chars x 7 rows");

void section12(counters& c) {
    section("Section 12 — the TRIPLE grid: 35 option TRIPLES x 19 types x 7 rows");

    const ImplPolicy& linked = linked_policy();

    // The OLD rule: the '0' option is validity-checked even when an
    // align option is present.
    ImplPolicy old_rule = linked;
    old_rule.zero_ignored_with_align = false;

    // Does this triple carry BOTH the align option and the '0' option?
    const auto is_align_zero = [](std::size_t i, std::size_t j, std::size_t k) {
        const auto has = [i, j, k](std::size_t slot) {
            return i == slot || j == slot || k == slot;
        };
        return has(kAlignSlot) && has(kZeroSlot);
    };

    std::size_t total = 0;
    std::size_t agree_new = 0;
    std::size_t agree_old = 0;
    std::size_t divergent = 0;
    std::size_t divergent_outside_family = 0;
    std::size_t family_cells = 0;
    std::vector<std::string> examples;

    // How many of the 35 triples carry BOTH the align option and the
    // '0' option?  The third slot can be any of the other five, so
    // five triples -- 5 x 19 x 7 = 665 cells.
    std::size_t family_triples = 0;
    for (std::size_t i = 0; i < kSlotCount; ++i) {
        for (std::size_t j = i + 1; j < kSlotCount; ++j) {
            for (std::size_t k = j + 1; k < kSlotCount; ++k) {
                if (is_align_zero(i, j, k)) ++family_triples;
            }
        }
    }

    const auto run = [&](const char* label, ArgCategory cat, auto& value) {
        for (std::size_t i = 0; i < kSlotCount; ++i) {
            for (std::size_t j = i + 1; j < kSlotCount; ++j) {
                for (std::size_t k = j + 1; k < kSlotCount; ++k) {
                    const std::string triple = std::string{kSlotText[i]}
                                             + kSlotText[j] + kSlotText[k];
                    const bool family = is_align_zero(i, j, k);
                    for (std::size_t t = 0; t < kTypeCount; ++t) {
                        const std::string spec = triple + kTypeChars[t].text;
                        const std::string fmt = "{:" + spec + "}";
                        std::string message;
                        const Outcome outcome = measure(fmt, value, message);
                        const bool stdlib_ok = (outcome == Outcome::Ok);

                        std::string reason_new;
                        std::string reason_old;
                        const bool model_new =
                            model_verdict(cat, spec, linked, reason_new);
                        const bool model_old =
                            model_verdict(cat, spec, old_rule, reason_old);

                        ++total;
                        if (family) ++family_cells;
                        if (model_new == stdlib_ok) ++agree_new;
                        if (model_old == stdlib_ok) ++agree_old;

                        if (model_old != stdlib_ok) {
                            ++divergent;
                            if (!family) ++divergent_outside_family;
                            if (examples.size() < 12) {
                                examples.push_back(
                                    std::string{"    "} + label + " | {:"
                                    + spec + "} | the OLD rule says "
                                    + (model_old ? "ACCEPT" : "REJECT")
                                    + ", today's rule says "
                                    + (model_new ? "ACCEPT" : "REJECT")
                                    + ", the stdlib says "
                                    + (stdlib_ok ? "ACCEPT" : "REJECT"));
                            }
                        }
                    }
                }
            }
        }
    };

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    run("bool",   ArgCategory::Boolean,       b);
    run("char",   ArgCategory::Character,     ch);
    run("int",    ArgCategory::Integral,      i);
    run("long",   ArgCategory::Integral,      l);
    run("double", ArgCategory::FloatingPoint, d);
    run("string", ArgCategory::String,        sv);
    run("voidp",  ArgCategory::Pointer,       vp);

    std::printf("  the grid: 35 option TRIPLES x 19 type chars x 7 rows"
                " = %zu cells\n", total);
    std::printf("  the linked stdlib is \"%s\"\n", linked.name);
    std::printf("  the model with TODAY's rule   vs the stdlib: %zu/%zu agree\n",
                agree_new, total);
    std::printf("  the model with the OLD rule    vs the stdlib: %zu/%zu agree\n",
                agree_old, total);
    std::printf("  the triples that carry the align option AND the '0':"
                " %zu of %zu (%zu cells)\n",
                family_triples, kTripleCount, family_cells);

    if (!examples.empty()) {
        std::printf("\n  the cells where the OLD rule disagrees with the stdlib:\n");
        for (const std::string& ex : examples) {
            std::printf("%s\n", ex.c_str());
        }
        std::printf("    -> every one of them is a triple that carries the align\n"
                    "       option AND the '0' option.  No rule needs THREE\n"
                    "       options to meet: the pair grid's answer is still\n"
                    "       the answer, one level down.\n");
    }

    c.expect_eq(total, kTripleCells, "the triple grid's cell count");
    c.expect_eq(total - agree_new, 0,
                "the model (today's rule) agrees with the linked stdlib");
    c.expect_eq(divergent_outside_family, 0,
                "every OLD-rule divergence is an align+zero triple");
    c.expect_eq(family_triples, 5u,
                "the triples that carry both the align and the '0'");
    c.expect_eq(family_cells, 5u * kTypeCount * kRowCount,
                "the align+zero family's cell count");
    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        c.expect_eq(divergent, 14u,
                    "the OLD rule's disagreements under libc++ 21");
    } else {
        c.expect_eq(divergent, 0,
                    "the OLD rule's disagreements under libstdc++ 15");
    }
}

// ============================================================================
// Section 13 — THE FILL x ALIGN x ZERO INTERSECTION.
//
// Nov 14's next step named this cell exactly: "the first cell that
// needs all THREE of fill, align and '0' at once is the interesting
// one -- the fill's Unicode rule and the align/zero rule have never
// met".  The fill's rule and the align/zero rule meet HERE, and this
// section asks whether they are INDEPENDENT.
//
// Three measurements:
//
//   13.1  Does the arc's model agree with the linked stdlib on every
//         cell of the intersection -- including the cells where the
//         fill is ILL-FORMED and the stdlib RECOVERS from it?
//   13.2  Is the recovery EQUIVALENT to the U+FFFD substitution?
//         Measured against the stdlib that recovers.
//   13.3  Does the ALIGN option survive the recovery?  The arc's
//         scanner used to say NO -- it consumed the alignment option
//         and did not record it -- which COUPLED the two rules.  That
//         is invisible in both shipped stdlibs, so it is measured with
//         a synthesized policy that has both halves.
// ============================================================================

// One fill-and-align variant.  The fill is given as bytes because it
// may be ill-formed; `len` is its byte length.
struct FillAlignVariant {
    const char* label;
    const char* fill;
    std::size_t len;
    bool has_align;
};

constexpr FillAlignVariant kFillAlignVariants[] = {
    {"no fill, no align",        "",                  0, false},
    {"no fill, align",           "",                  0, true},
    {"an ASCII fill",            "*",                 1, true},
    {"a 4-byte fill (U+1F600)",  "\xF0\x9F\x98\x80",  4, true},
    {"an ill-formed 1-byte fill", "\x80",              1, true},
    {"an ill-formed 2-byte fill", "\xE2\x82",          2, true},
    {"an ill-formed overlong fill", "\xC0\xAF",        2, true},
};

static_assert(sizeof(kFillAlignVariants) / sizeof(kFillAlignVariants[0]) == 7u,
              "seven fill-and-align variants");

// The U+FFFD that a recovery substitutes, as UTF-8 bytes.
constexpr const char* kReplacementFill = "\xEF\xBF\xBD";
constexpr std::size_t kReplacementFillLen = 3u;

void section13(counters& c) {
    section("Section 13 — the FILL x ALIGN x ZERO intersection");

    const ImplPolicy& linked = linked_policy();

    // ---- 13.1 the intersection, measured against the linked stdlib ----
    //
    // Every cell of the grid is: a fill-and-align variant, with or
    // without the '0' option, with a type char, for each argument row.
    std::size_t total = 0;
    std::size_t agree = 0;
    std::size_t recovered_cells = 0;
    std::vector<std::string> examples;

    const auto run = [&](const char* label, ArgCategory cat, auto& value) {
        for (const FillAlignVariant& v : kFillAlignVariants) {
            // Is this fill one that the LINKED implementation recovers
            // from?  If it is, the cell is one where the recovery path
            // runs -- which is where the arc's scanner used to lose the
            // alignment option.
            const Utf8CodePoint cp =
                decode_utf8(std::string_view{v.fill, v.len});
            const bool recoverable =
                v.len > 0 && cp.fault != FillFault::None
                && !linked.fill.is_fatal(cp.fault);

            for (int zero = 0; zero < 2; ++zero) {
                for (std::size_t t = 0; t < kTypeCount; ++t) {
                    std::string spec{v.fill, v.len};
                    if (v.has_align) spec += '<';
                    if (zero != 0) spec += '0';
                    spec += kTypeChars[t].text;
                    const std::string fmt = "{:" + spec + "}";
                    std::string message;
                    const Outcome outcome = measure(fmt, value, message);
                    const bool stdlib_ok = (outcome == Outcome::Ok);

                    std::string reason;
                    const bool model_ok =
                        model_verdict(cat, spec, linked, reason);

                    ++total;
                    if (model_ok == stdlib_ok) ++agree;
                    if (recoverable) ++recovered_cells;

                    if (model_ok != stdlib_ok && examples.size() < 8) {
                        examples.push_back(
                            std::string{"    "} + label + " | {:"
                            + spec + "} | the model says "
                            + (model_ok ? "ACCEPT" : "REJECT")
                            + ", the stdlib says "
                            + (stdlib_ok ? "ACCEPT" : "REJECT"));
                    }
                }
            }
        }
    };

    bool b = true;
    char ch = 'A';
    int i = 42;
    long l = 42L;
    double d = 1.5;
    std::string_view sv = "s";
    const void* vp = nullptr;

    run("bool",   ArgCategory::Boolean,       b);
    run("char",   ArgCategory::Character,     ch);
    run("int",    ArgCategory::Integral,      i);
    run("long",   ArgCategory::Integral,      l);
    run("double", ArgCategory::FloatingPoint, d);
    run("string", ArgCategory::String,        sv);
    run("voidp",  ArgCategory::Pointer,       vp);

    std::printf("  the intersection: 7 fill-and-align variants x 2 ('0' or not)"
                " x 19 types x 7 rows = %zu cells\n", total);
    std::printf("  the linked stdlib is \"%s\"\n", linked.name);
    std::printf("  the model agrees with the stdlib on %zu/%zu cells\n",
                agree, total);
    std::printf("  the cells where the stdlib RECOVERS from an ill-formed"
                " fill: %zu\n", recovered_cells);
    if (!examples.empty()) {
        std::printf("\n  the cells where the model and the stdlib disagree:\n");
        for (const std::string& ex : examples) {
            std::printf("%s\n", ex.c_str());
        }
    }

    c.expect_eq(total, 7u * 2u * kTypeCount * kRowCount,
                "the intersection's cell count");
    c.expect_eq(total - agree, 0,
                "the model agrees with the linked stdlib on the intersection");
    if (std::strcmp(linked.name, "libc++ 21") == 0) {
        c.expect_eq(recovered_cells, 0,
                    "libc++ 21 recovers from nothing");
    } else {
        c.check(recovered_cells > 0);
    }

    // ---- 13.2 the recovery is a U+FFFD substitution ----------------------
    //
    // If the stdlib's recovery really substitutes U+FFFD for the
    // maximal subpart and carries on, then a spec with an ill-formed
    // fill must be judged exactly like the SAME spec with the fill
    // replaced by U+FFFD -- because after the substitution the two
    // specs are the same spec.
    {
        std::size_t checked = 0;
        std::size_t disagree = 0;
        for (const FillAlignVariant& v : kFillAlignVariants) {
            if (v.len == 0) continue;
            const Utf8CodePoint cp =
                decode_utf8(std::string_view{v.fill, v.len});
            if (cp.fault == FillFault::None) continue;
            if (linked.fill.is_fatal(cp.fault)) continue;   // not recovered

            for (int zero = 0; zero < 2; ++zero) {
                for (std::size_t t = 0; t < kTypeCount; ++t) {
                    std::string ill{v.fill, v.len};
                    std::string good{kReplacementFill, kReplacementFillLen};
                    if (v.has_align) { ill += '<'; good += '<'; }
                    if (zero != 0) { ill += '0'; good += '0'; }
                    ill += kTypeChars[t].text;
                    good += kTypeChars[t].text;

                    std::string m1;
                    std::string m2;
                    const bool ok_ill = measure("{:"
                                                + ill, sv, m1) == Outcome::Ok;
                    const bool ok_good = measure("{:"
                                                 + good, sv, m2) == Outcome::Ok;
                    ++checked;
                    if (ok_ill != ok_good) ++disagree;
                }
            }
        }
        std::printf("\n  the recovery is a U+FFFD substitution:\n");
        std::printf("    the ill-formed fill and the U+FFFD fill were compared"
                    " on %zu cells\n", checked);
        std::printf("    the stdlib judged them differently on %zu of them\n",
                    disagree);
        c.expect_eq(disagree, 0,
                    "a recovered fill is judged like the U+FFFD it becomes");
        if (std::strcmp(linked.name, "libc++ 21") == 0) {
            c.expect_eq(checked, 0,
                        "libc++ 21 never recovers, so there is nothing to compare");
        } else {
            c.check(checked > 0);
        }
    }

    // ---- 13.3 does the align survive the recovery? ----------------------
    //
    // The SYNTHESIZED policy: it RECOVERS from an ill-formed fill (like
    // libstdc++ 15) AND it IGNORES the '0' when an align option is
    // present (like libc++ 21).  No shipped stdlib has both halves, so
    // the coupling between the two rules is invisible in both -- but the
    // arc's scanner has to be right about it anyway, because the
    // scanner models the RULES and the rules are the rules.
    {
        ImplPolicy synth = linked;
        synth.fill.fatal = kFaultOverlong | kFaultSurrogate | kFaultOutOfRange;
        synth.zero_ignored_with_align = true;

        // The scanner as it was on Nov 14: the alignment option was
        // consumed and forgotten.
        ImplPolicy synth_old = synth;
        synth_old.fill.recovery_keeps_align = false;

        std::size_t checked = 0;
        std::size_t coupled = 0;
        std::size_t invariant_violations = 0;
        std::vector<std::string> coupled_examples;

        for (int zero = 0; zero < 2; ++zero) {
            for (std::size_t t = 0; t < kTypeCount; ++t) {
                for (const char* fill : {"\x80", "\xE2\x82"}) {
                    std::string spec{fill};
                    spec += '<';
                    if (zero != 0) spec += '0';
                    spec += kTypeChars[t].text;

                    // The same spec with the fill already substituted.
                    std::string good{kReplacementFill, kReplacementFillLen};
                    good += '<';
                    if (zero != 0) good += '0';
                    good += kTypeChars[t].text;

                    std::string r1;
                    std::string r2;
                    std::string r3;
                    const bool new_verdict =
                        model_verdict(ArgCategory::Boolean, spec, synth, r1);
                    const bool old_verdict =
                        model_verdict(ArgCategory::Boolean, spec, synth_old, r2);
                    const bool reference =
                        model_verdict(ArgCategory::Boolean, good, synth, r3);

                    ++checked;
                    if (new_verdict != old_verdict) {
                        ++coupled;
                        if (coupled_examples.size() < 6) {
                            coupled_examples.push_back(
                                std::string("    {:"
                                + spec
                                + "} | Nov 14's scanner says "
                                + (old_verdict ? "ACCEPT" : "REJECT")
                                + ", today's scanner says "
                                + (new_verdict ? "ACCEPT" : "REJECT")
                                + ", the U+FFFD reference says "
                                + (reference ? "ACCEPT" : "REJECT")));
                        }
                    }
                    if (new_verdict != reference) ++invariant_violations;
                }
            }
        }

        std::printf("\n  the SYNTHESIZED policy (recovers AND ignores the '0'"
                    " with an align):\n");
        std::printf("    the cells where Nov 14's scanner and today's scanner"
                    " differ: %zu/%zu\n", coupled, checked);
        for (const std::string& ex : coupled_examples) {
            std::printf("%s\n", ex.c_str());
        }
        std::printf("    -> on the recovery path the scanner used to consume\n"
                    "       the alignment option and NOT record it, so the\n"
                    "       '0' rule saw no align option at all.  The two\n"
                    "       rules were NOT independent: the fill's rule\n"
                    "       changed the zero's rule's answer.\n");

        c.expect_eq(checked, 2u * 2u * kTypeCount,
                    "the synthesized policy's cell count");
        c.check(coupled > 0);
        c.expect_eq(invariant_violations, 0,
                    "today's scanner agrees with the U+FFFD reference");
    }
}

// ============================================================================
// Section 14 — THE FILL'S BEHAVIOUR SPACE.
//
// For the first time in the arc, the grid's axis is the POLICY, not the
// spec.
//
// Every previous grid asked the same shape of question: "does the arc's
// model agree with the linked stdlib on every SPEC?"  Today's grid asks a
// different one: "how many distinct BEHAVIOURS can the fill's enforcement
// express?"
//
// The fill's enforcement is ONE value -- FillBehaviour -- with three
// fields:
//
//   unicode               the RULE: byte-oriented, or a Unicode scalar
//   fatal                 a five-bit MASK: which ill-formed fills are
//                         fatal (the rest are recovered from)
//   recovery_keeps_align  the recovery rule: does the alignment option
//                         that follows a recovered fill survive?
//
// 2 x 32 x 2 = 128 candidate values.  Each is scored by its SIGNATURE:
// its verdict on every cell of one fixed battery.
//
// Four measurements:
//
//   14.1  the space: how many distinct signatures?
//   14.2  the classes: which candidates behave identically?
//   14.3  the DEAD bits: which of the five fault bits can change a
//         verdict at all?
//   14.4  the COUPLING: the recovery rule is observable only under the
//         C++20 wording of the align/zero rule -- so the fill's value is
//         NOT measurable on its own.
//   14.5  the linked stdlib, on the same battery.
// ============================================================================

// One fill variant: the bytes, and the fault the decoder reports for
// them.  The bytes carry their length, because an ill-formed sequence
// is not a C string.
struct FillVariant {
    const char* label;
    const char* bytes;
    std::size_t len;
    FillFault fault;
};

// One argument: a category, a type char, and the pack that stands for
// it.  The PAIR matters, not the category alone: the '0' option is
// valid for some (category, type) pairs and not for others, and that is
// what makes the alignment option observable at all.
struct ArgPair {
    ArgCategory category;
    char type;
    Pack pack;
    const char* label;
};

// One cell of the battery: a spec (without the braces) and the argument
// it is judged against.
struct FillCell {
    std::string spec;
    ArgCategory category;
    Pack pack;
    const char* fill_label;
};

// One candidate of the fill's enforcement, under one align/zero wording.
struct FillPoint {
    FillBehaviour fill;
    bool zero_ignored;
    std::string signature;
};

void section14(counters& c) {
    section("Section 14 — the fill's BEHAVIOUR SPACE");

    // ---- the battery ---------------------------------------------------
    //
    // Every cell is a fill, an alignment option, a '0' option and an
    // argument.  The fill is the whole prefix of the spec, so the
    // recovery path is reachable only where the fill is immediately
    // followed by the alignment option.
    const FillVariant variants[] = {
        {"an ASCII fill",           "*",                1, FillFault::None},
        {"a 4-byte fill (U+1F600)", "\xF0\x9F\x98\x80", 4, FillFault::None},
        {"an invalid-lead fill",    "\x80",             1, FillFault::InvalidLead},
        {"a truncated 3-byte fill", "\xE2\x82",         2, FillFault::Truncated},
        {"an overlong 2-byte fill", "\xC0\xAF",         2, FillFault::Overlong},
        {"a surrogate fill",        "\xED\xA0\x80",     3, FillFault::Surrogate},
        {"an above-U+10FFFF fill",  "\xF4\x90\x80\x80", 4, FillFault::OutOfRange},
    };
    const ArgPair args[] = {
        {ArgCategory::Boolean,       's', Pack::OneBool,   "a bool"},
        {ArgCategory::Character,     'c', Pack::OneChar,   "a char"},
        {ArgCategory::Integral,      'd', Pack::OneInt,    "an int"},
        {ArgCategory::FloatingPoint, 'f', Pack::OneDouble, "a double"},
        {ArgCategory::String,        's', Pack::OneString, "a string"},
    };

    std::vector<FillCell> battery;
    for (const FillVariant& v : variants) {
        for (int align = 0; align < 2; ++align) {
            for (int zero = 0; zero < 2; ++zero) {
                for (const ArgPair& a : args) {
                    std::string spec{v.bytes, v.len};
                    if (align != 0) spec += '<';
                    if (zero != 0) spec += '0';
                    spec += a.type;
                    battery.push_back(
                        FillCell{std::move(spec), a.category, a.pack, v.label});
                }
            }
        }
    }
    std::printf("  the battery: %zu fills x 2 (align or not) x 2 ('0' or not)"
                " x %zu arguments = %zu cells\n",
                sizeof(variants) / sizeof(variants[0]),
                sizeof(args) / sizeof(args[0]), battery.size());

    // ---- 14.1 the space ------------------------------------------------
    //
    // Everything except the fill's enforcement and the align/zero
    // wording is pinned to the STANDARD's policy, so the measurement is
    // the same under both toolchains.
    const ImplPolicy base = kImpls[impl_index_of("standard")];

    const auto signature_of = [&](const FillBehaviour& fill, bool zero_ignored) {
        ImplPolicy p = base;
        p.fill = fill;
        p.zero_ignored_with_align = zero_ignored;
        std::string sig;
        sig.reserve(battery.size());
        for (const FillCell& cell : battery) {
            std::string reason;
            sig += model_verdict(cell.category, cell.spec, p, reason) ? '1' : '0';
        }
        return sig;
    };

    std::vector<FillPoint> points;
    for (int u = 0; u < 2; ++u) {
        for (std::uint32_t m = 0; m < 32u; ++m) {
            for (int r = 0; r < 2; ++r) {
                for (int z = 0; z < 2; ++z) {
                    FillBehaviour fb{};
                    fb.unicode = (u != 0);
                    fb.fatal = static_cast<std::uint8_t>(m);
                    fb.recovery_keeps_align = (r != 0);
                    const bool zi = (z != 0);
                    points.push_back(FillPoint{fb, zi, signature_of(fb, zi)});
                }
            }
        }
    }
    c.expect_eq(points.size(), 256u, "the candidate space");

    const auto distinct_under = [&](bool zero_ignored) {
        std::vector<std::string> seen;
        for (const FillPoint& p : points) {
            if (p.zero_ignored != zero_ignored) continue;
            bool found = false;
            for (const std::string& s : seen) {
                if (s == p.signature) { found = true; break; }
            }
            if (!found) seen.push_back(p.signature);
        }
        return seen.size();
    };
    std::size_t distinct_all = 0;
    {
        std::vector<std::string> seen;
        for (const FillPoint& p : points) {
            bool found = false;
            for (const std::string& s : seen) {
                if (s == p.signature) { found = true; break; }
            }
            if (!found) { seen.push_back(p.signature); ++distinct_all; }
        }
    }
    const std::size_t d23 = distinct_under(false);
    const std::size_t d20 = distinct_under(true);
    std::printf("  the space: 128 candidate values (2 rules x 32 fatal masks"
                " x 2 recovery rules)\n");
    std::printf("  the behaviours:\n");
    std::printf("    under the C++23 align/zero wording: %zu\n", d23);
    std::printf("    under the C++20 align/zero wording: %zu\n", d20);
    std::printf("    over the whole space:              %zu\n", distinct_all);
    c.expect_eq(d23 + d20, distinct_all,
                "the two wordings' behaviours do not overlap");

    // ---- 14.2 the classes ----------------------------------------------
    //
    // Each class is a set of candidates that behave IDENTICALLY on every
    // cell.  The tag names what the members have in COMMON -- and when
    // the members disagree about the recovery rule while behaving the
    // same, that disagreement is the coupling, printed.
    const auto tag_of = [&](const FillPoint& p) {
        std::string tag = p.zero_ignored ? "C++20 wording" : "C++23 wording";
        if (!p.fill.unicode) {
            tag += ", byte rule (the enforcement is inert)";
            return tag;
        }
        tag += ", scalar rule, truncated=";
        tag += ((p.fill.fatal & kFaultTruncated) != 0) ? "fatal" : "recoverable";
        tag += ", invalid-lead=";
        tag += ((p.fill.fatal & kFaultInvalidLead) != 0) ? "fatal" : "recoverable";
        tag += ", recovery=";
        tag += p.fill.recovery_keeps_align ? "keeps the align" : "loses the align";
        return tag;
    };

    std::vector<std::string> class_sigs;
    std::vector<std::vector<std::size_t>> class_members;
    for (std::size_t i = 0; i < points.size(); ++i) {
        bool found = false;
        for (std::size_t k = 0; k < class_sigs.size(); ++k) {
            if (class_sigs[k] == points[i].signature) {
                class_members[k].push_back(i);
                found = true;
                break;
            }
        }
        if (!found) {
            class_sigs.push_back(points[i].signature);
            class_members.push_back(std::vector<std::size_t>{i});
        }
    }
    std::printf("\n  the classes (%zu of them):\n", class_sigs.size());
    for (std::size_t k = 0; k < class_sigs.size(); ++k) {
        std::vector<std::string> tags;
        for (std::size_t m : class_members[k]) {
            const std::string tag = tag_of(points[m]);
            bool found = false;
            for (const std::string& t : tags) {
                if (t == tag) { found = true; break; }
            }
            if (!found) tags.push_back(tag);
        }
        std::printf("    class %2zu: %3zu candidates\n", k, class_members[k].size());
        for (const std::string& t : tags) {
            std::printf("        %s\n", t.c_str());
        }
    }

    // ---- 14.3 the DEAD bits --------------------------------------------
    //
    // Toggle ONE bit of the fatal mask and count the candidates whose
    // behaviour changes.  A bit that changes nothing is DEAD: the
    // enforcement spends a bit it can never spend.
    struct FaultBit {
        const char* name;
        std::uint8_t bit;
    };
    const FaultBit fault_bits[] = {
        {"truncated",    kFaultTruncated},
        {"invalid lead", kFaultInvalidLead},
        {"overlong",     kFaultOverlong},
        {"surrogate",    kFaultSurrogate},
        {"out of range", kFaultOutOfRange},
    };
    std::size_t changed_counts[5] = {0, 0, 0, 0, 0};
    std::size_t live_bits = 0;
    std::printf("\n  the five fault bits, toggled one at a time:\n");
    for (std::size_t i = 0; i < 5; ++i) {
        const FaultBit& fb = fault_bits[i];
        std::size_t changed = 0;
        std::size_t cells = 0;
        for (const FillPoint& p : points) {
            FillBehaviour f = p.fill;
            f.fatal = static_cast<std::uint8_t>(f.fatal ^ fb.bit);
            const std::string sig = signature_of(f, p.zero_ignored);
            if (sig == p.signature) continue;
            ++changed;
            for (std::size_t k = 0; k < sig.size(); ++k) {
                if (sig[k] != p.signature[k]) ++cells;
            }
        }
        changed_counts[i] = changed;
        if (changed > 0) ++live_bits;
        std::printf("    %-14s %3zu of 256 candidates change (%zu cells)\n",
                    fb.name, changed, cells);
    }
    c.expect_eq(live_bits, 2u, "exactly two of the five fault bits are live");
    c.expect_eq(changed_counts[2], 0u, "the overlong bit is dead");
    c.expect_eq(changed_counts[3], 0u, "the surrogate bit is dead");
    c.expect_eq(changed_counts[4], 0u, "the out-of-range bit is dead");

    // ---- 14.4 the COUPLING ---------------------------------------------
    //
    // Toggle the recovery rule and count the candidates that change.  The
    // answer depends on the OTHER per-implementation fact: the alignment
    // option is read at exactly ONE place in the scanner -- the '0'
    // option's validity -- and the C++23 wording does not read it there.
    std::size_t rec_changed[2] = {0, 0};
    for (int z = 0; z < 2; ++z) {
        const bool zi = (z != 0);
        for (const FillPoint& p : points) {
            if (p.zero_ignored != zi) continue;
            FillBehaviour f = p.fill;
            f.recovery_keeps_align = !f.recovery_keeps_align;
            if (signature_of(f, zi) != p.signature) ++rec_changed[z];
        }
    }
    std::printf("\n  the recovery rule, toggled on its own:\n");
    std::printf("    under the C++23 wording: %zu of 128 candidates change\n",
                rec_changed[0]);
    std::printf("    under the C++20 wording: %zu of 128 candidates change\n",
                rec_changed[1]);
    c.expect_eq(rec_changed[0], 0u,
                "the recovery rule is inert under the C++23 wording");
    c.check(rec_changed[1] > 0);

    // ---- 14.5 the linked stdlib ----------------------------------------
    //
    // The model must still agree with the LINKED stdlib on every cell of
    // the battery, under the linked implementation's own policy.
    {
        const ImplPolicy& linked = linked_policy();
        std::size_t agree = 0;
        std::vector<std::string> bad;
        for (const FillCell& cell : battery) {
            const ProbeResult pr = probe_stdlib("{:" + cell.spec + "}", cell.pack);
            std::string reason;
            const bool model_ok =
                model_verdict(cell.category, cell.spec, linked, reason);
            if (model_ok == pr.accepted) {
                ++agree;
            } else if (bad.size() < 6) {
                bad.push_back("    " + std::string{cell.fill_label} + " | the model"
                              " says " + (model_ok ? "ACCEPT" : "REJECT") +
                              ", the stdlib says " +
                              (pr.accepted ? "ACCEPT" : "REJECT") + " | " + pr.message);
            }
        }
        std::printf("\n  the linked stdlib (\"%s\"): the model agrees on %zu of"
                    " %zu cells\n", linked.name, agree, battery.size());
        for (const std::string& s : bad) std::printf("%s\n", s.c_str());
        c.expect_eq(agree, battery.size(),
                    "the model agrees with the linked stdlib on the battery");
    }
}

// ============================================================================
// Section 15 — THE FIELD WIDTH.
//
// Nov 13 measured "the width counts code points, not bytes" and that
// measurement was RIGHT -- for its battery.  Every code point in that
// battery was one column wide, so "code points" and "columns" said the
// same thing and the battery could not tell them apart.
//
// Today's battery can.  It holds wide code points (U+1F600, U+4E00,
// U+FF21), combining marks, variation selectors and ZWJ sequences, and
// on it the three candidate units come apart:
//
//   bytes        agree on  1 of 28 cells
//   code points  agree on 11 of 28 cells
//   COLUMNS      agree on 28 of 28 cells
//
// The rule is [format.string.std]/12: the width is the sum of the field
// widths of the FIRST code point of each extended grapheme cluster, and
// a code point is worth one column or two.
//
// Four measurements:
//   15.1  the census: bytes vs code points vs columns, cell by cell.
//   15.2  the fill is assumed to be ONE column wide, so a wide fill
//         overflows the field it is padding.
//   15.3  the recovery's substitution count: ONE U+FFFD for the whole
//         maximal subpart -- measured, not assumed.
//   15.4  the linked stdlib, on the same battery.
// ============================================================================

// ---- the field-width model (NEW TODAY). ----------------------------------
//
// [format.string.std]/12 (C++26's wording; C++23's /13 says the same
// thing under a different paragraph number):
//
//   "For a sequence of characters in UTF-8, UTF-16, or UTF-32, an
//    implementation should use as its field width the sum of the field
//    widths of the first code point of each extended grapheme cluster."
//
// So the width is counted in COLUMNS.  A code point is worth ONE column
// or TWO, and the unit is the CLUSTER.
//
// The width-2 set has two parts: [format.string.std]/12 names three
// ranges explicitly, and everything else whose East_Asian_Width is "W"
// or "F".  The second part is a TABLE -- libc++ 21's has 110 entries --
// so the arc carries a RESTRICTED one: the ranges the battery needs,
// plus the three the standard names.  Section 15 MEASURES the model
// against the linked stdlib, so the restriction is visible rather than
// assumed.
struct WidthRange {
    char32_t lo;
    char32_t hi;
};

constexpr WidthRange kWidth2Ranges[] = {
    {0x1100u, 0x115Fu},   // Hangul Jamo
    {0x231Au, 0x231Bu},   // watch
    {0x2329u, 0x232Au},   // angle brackets
    {0x23E9u, 0x23ECu},   // media control
    {0x23F0u, 0x23F0u},
    {0x23F3u, 0x23F3u},
    {0x25FDu, 0x25FEu},   // geometric shapes
    {0x2614u, 0x2615u},   // umbrella / hot beverage
    {0x2648u, 0x2653u},   // zodiac
    {0x267Fu, 0x267Fu},   // infinity
    {0x2693u, 0x2693u},   // anchor
    {0x26A1u, 0x26A1u},   // high voltage
    {0x26AAu, 0x26ABu},   // medium circles
    {0x26BDu, 0x26BEu},   // soccer / baseball
    {0x26C4u, 0x26C5u},   // chess pieces
    {0x26CEu, 0x26CEu},   // ophiuchus
    {0x26D4u, 0x26D4u},   // no entry
    {0x26EAu, 0x26EAu},   // church
    {0x26F2u, 0x26F3u},   // fountain / golf
    {0x26F5u, 0x26F5u},   // sailboat
    {0x26FAu, 0x26FAu},   // tent
    {0x26FDu, 0x26FDu},   // fuel pump
    {0x2705u, 0x2705u},   // check mark
    {0x270Au, 0x270Bu},   // fists
    {0x2728u, 0x2728u},   // sparkles
    {0x274Cu, 0x274Cu},   // cross mark
    {0x274Eu, 0x274Eu},   // cross mark button
    {0x2753u, 0x2755u},   // question / exclamation
    {0x2757u, 0x2757u},   // heavy exclamation
    {0x2795u, 0x2797u},   // heavy plus / minus / division
    {0x27B0u, 0x27B0u},   // curly loop
    {0x27BFu, 0x27BFu},   // double curly loop
    {0x2B1Bu, 0x2B1Cu},   // black and white squares
    {0x2B50u, 0x2B50u},   // star
    {0x2B55u, 0x2B55u},   // hollow red circle
    {0x2E80u, 0x303Eu},   // CJK radicals .. CJK punctuation
    {0x3041u, 0x33FFu},   // kana .. CJK compatibility
    {0x3400u, 0x4DBFu},   // CJK extension A
    {0x4DC0u, 0x4DFFu},   // Yijing hexagrams  ([format.string.std]/12)
    {0x4E00u, 0x9FFFu},   // CJK unified ideographs
    {0xA000u, 0xA4CFu},   // Yi syllables
    {0xAC00u, 0xD7A3u},   // Hangul syllables
    {0xF900u, 0xFAFFu},   // CJK compatibility ideographs
    {0xFE10u, 0xFE19u},   // vertical forms
    {0xFE30u, 0xFE6Fu},   // CJK compatibility forms
    {0xFF01u, 0xFF60u},   // fullwidth forms
    {0xFFE0u, 0xFFE6u},   // fullwidth signs
    {0x1F300u, 0x1F5FFu}, // Misc Symbols and Pictographs (/12)
    {0x1F600u, 0x1F64Fu}, // emoticons
    {0x1F680u, 0x1F6FFu}, // transport and map
    {0x1F900u, 0x1F9FFu}, // Supplemental Symbols and Pictographs (/12)
    {0x1FA70u, 0x1FAFFu}, // symbols and pictographs extended-A
    {0x20000u, 0x2FFFDu}, // plane 2 defaults to W
    {0x30000u, 0x3FFFDu}, // plane 3 defaults to W
};

// Is this code point worth TWO columns?
[[nodiscard]] constexpr bool is_width_two(char32_t cp) noexcept {
    for (const WidthRange& r : kWidth2Ranges) {
        if (cp >= r.lo && cp <= r.hi) return true;
    }
    return false;
}

// The EXTEND set, restricted to the ranges the battery needs: the code
// points that join the PRECEDING grapheme cluster.
[[nodiscard]] constexpr bool is_cluster_extend(char32_t cp) noexcept {
    return (cp >= 0x0300u && cp <= 0x036Fu) ||   // combining diacritics
           (cp >= 0x1AB0u && cp <= 0x1AFFu) ||   // combining ext. A
           (cp >= 0x1DC0u && cp <= 0x1DFFu) ||   // combining supplement
           (cp >= 0x20D0u && cp <= 0x20F0u) ||   // combining for symbols
           (cp >= 0xFE00u && cp <= 0xFE0Fu) ||   // variation selectors
           (cp >= 0xFE20u && cp <= 0xFE2Fu) ||   // combining half marks
           (cp >= 0x1F3FBu && cp <= 0x1F3FFu) || // emoji modifiers
           (cp >= 0xE0100u && cp <= 0xE01EFu) || // variation selectors supp.
           (cp >= 0x094Du && cp <= 0x094Du) ||   // Devanagari virama
           (cp >= 0x09CDu && cp <= 0x09CDu) ||   // Bengali virama
           (cp >= 0x0ACDu && cp <= 0x0ACDu) ||   // Gujarati virama
           (cp >= 0x0B4Du && cp <= 0x0B4Du) ||   // Oriya virama
           (cp >= 0x0C4Du && cp <= 0x0C4Du) ||   // Telugu virama
           (cp >= 0x0CCDu && cp <= 0x0CCDu) ||   // Kannada virama
           (cp >= 0x0D4Du && cp <= 0x0D4Du);     // Malayalam virama
}

constexpr char32_t kZwj = 0x200Du;

// ---- the EXTENDED PICTOGRAPHIC set (carried forward from Nov 18) -------
//
// UAX #29's GB11:  \p{Extended_Pictographic} Extend* ZWJ x
//                  \p{Extended_Pictographic}
//
// so the code point after a ZWJ joins the cluster ONLY when both sides
// of the ZWJ are pictographic.  RESTRICTED, like the width's table: the
// ranges the battery needs, plus the obvious pictographs.  Section 17
// MEASURES the model against the linked stdlib, so the restriction is
// visible rather than assumed.
constexpr WidthRange kExtendedPictographic[] = {
    {0x00A9u, 0x00A9u},   // copyright sign
    {0x00AEu, 0x00AEu},   // registered sign
    {0x203Cu, 0x203Cu},   // double exclamation mark
    {0x2049u, 0x2049u},   // exclamation question mark
    {0x2122u, 0x2122u},   // trade mark sign
    {0x2139u, 0x2139u},   // information source
    {0x2194u, 0x21AAu},   // arrows
    {0x231Au, 0x231Bu},   // watch, hourglass
    {0x2328u, 0x2328u},   // hourglass
    {0x23CFu, 0x23FAu},   // media controls
    {0x24C2u, 0x24C2u},   // circled latin capital M
    {0x25AAu, 0x25FEu},   // geometric shapes
    {0x2600u, 0x27BFu},   // misc symbols + dingbats (U+2764 HEART)
    {0x2934u, 0x2935u},   // arrows
    {0x2B05u, 0x2B07u},   // triangles, squares
    {0x2B1Bu, 0x2B1Cu},   // black and white squares
    {0x2B50u, 0x2B50u},   // white medium star
    {0x2B55u, 0x2B55u},   // heavy large circle
    {0x3030u, 0x3030u},   // wavy dash
    {0x303Du, 0x303Du},   // part alternation mark
    {0x3297u, 0x3297u},   // circled ideograph congratulation
    {0x3299u, 0x3299u},   // circled ideograph secret
    {0x1F000u, 0x1F02Fu}, // mahjong tiles, dominoes
    {0x1F0A0u, 0x1F0FFu}, // playing cards
    {0x1F100u, 0x1F1FFu}, // enclosed alphanumeric supplement
    {0x1F200u, 0x1F2FFu}, // enclosed ideographic supplement
    {0x1F300u, 0x1F5FFu}, // misc symbols and pictographs
    {0x1F600u, 0x1F64Fu}, // emoticons
    {0x1F680u, 0x1F6FFu}, // transport and map
    {0x1F7E0u, 0x1F7FFu}, // geometric shapes extended
    {0x1F900u, 0x1F9FFu}, // supplemental symbols and pictographs
    {0x1FA70u, 0x1FAFFu}, // symbols and pictographs extended-A
};

// Is this code point an Extended_Pictographic?
[[nodiscard]] constexpr bool is_extended_pictographic(char32_t cp) noexcept {
    for (const WidthRange& r : kExtendedPictographic) {
        if (cp >= r.lo && cp <= r.hi) return true;
    }
    return false;
}

// ---- the PREPEND set (NEW TODAY) ---------------------------------------
//
// UAX #29's GB9b:  Prepend x
//
// so a PREPEND code point joins the FOLLOWING cluster -- and a cluster is
// worth the width of its FIRST code point, so the prepend character's
// width is what the cluster costs.  RESTRICTED, like the arc's other
// tables: the ranges the battery needs.
constexpr WidthRange kPrepend[] = {
    {0x0600u, 0x0605u},   // Arabic number signs
    {0x06DDu, 0x06DDu},   // Arabic end of ayah
    {0x070Fu, 0x070Fu},   // Syriac abbreviation mark
    {0x0890u, 0x0891u},   // Arabic pound / piastre marks
    {0x08E2u, 0x08E2u},   // Arabic disputed end of ayah
    {0x0D4Eu, 0x0D4Eu},   // Malayalam letter dot reph
    {0x110BDu, 0x110BDu}, // Kaithi number sign
    {0x110CDu, 0x110CDu}, // Kaithi number sign above
    {0x111C2u, 0x111C3u}, // Sharada signs
    {0x1193Fu, 0x1193Fu}, // Dives Akuru prefix
    {0x11941u, 0x11941u}, // Dives Akuru prefix
    {0x11A3Au, 0x11A3Au}, // Zanabazar Square cluster separator
    {0x11A84u, 0x11A89u}, // Soyombo signs
    {0x11D46u, 0x11D46u}, // Masaram Gondi reph
    {0x11F02u, 0x11F02u}, // Kawi sign reph
};

// Is this code point a Prepend?
[[nodiscard]] constexpr bool is_prepend(char32_t cp) noexcept {
    for (const WidthRange& r : kPrepend) {
        if (cp >= r.lo && cp <= r.hi) return true;
    }
    return false;
}

// ---- the REGIONAL INDICATOR set (NEW TODAY) --------------------------
//
// UAX #29's GB12/GB13:  sot (RI RI)* RI x RI
//
// so REGIONAL INDICATORS pair up: the two halves of a flag are ONE
// cluster, and a cluster is worth the width of its FIRST code point.
[[nodiscard]] constexpr bool is_regional_indicator(char32_t cp) noexcept {
    return cp >= 0x1F1E6u && cp <= 0x1F1FFu;
}

// ---- the INDIC CONJUNCT sets (NEW TODAY) -----------------------------
//
// UAX #29's GB9c:  \p{InCB=Consonant} [Extend|Linker]* Linker
//                  [Extend|Linker]* x \p{InCB=Consonant}
//
// so a consonant, a LINKER (the virama) and another consonant form ONE
// cluster.  RESTRICTED: the scripts the battery needs.
constexpr WidthRange kIndicLinker[] = {
    {0x094Du, 0x094Du},   // Devanagari virama
    {0x09CDu, 0x09CDu},   // Bengali virama
    {0x0ACDu, 0x0ACDu},   // Gujarati virama
    {0x0B4Du, 0x0B4Du},   // Oriya virama
    {0x0C4Du, 0x0C4Du},   // Telugu virama
    {0x0CCDu, 0x0CCDu},   // Kannada virama
    {0x0D3Bu, 0x0D3Bu},   // Malayalam vertical bar virama
    {0x0D4Du, 0x0D4Du},   // Malayalam virama
    {0x1B44u, 0x1B44u},   // Balinese adeg adeg
    {0xA9C0u, 0xA9C0u},   // Javanese pangkon
    {0x11A34u, 0x11A35u}, // Zanabazar Square virama
    {0x11D44u, 0x11D45u}, // Masaram Gondi virama
    {0x11D97u, 0x11D97u}, // Gunjala Gondi virama
};

constexpr WidthRange kIndicConsonant[] = {
    {0x0915u, 0x0939u},   // Devanagari ka .. ha
    {0x0958u, 0x095Fu},   // Devanagari additional consonants
    {0x0978u, 0x097Fu},   // Devanagari extended consonants
    {0x0995u, 0x09A8u},   // Bengali ka .. qa
    {0x09AAu, 0x09B0u},   // Bengali pa .. ra
    {0x09B2u, 0x09B2u},   // Bengali la
    {0x09B6u, 0x09B9u},   // Bengali sha .. ha
    {0x09DCu, 0x09DDu},   // Bengali ru / rr
    {0x09DFu, 0x09DFu},   // Bengali yy
    {0x0A95u, 0x0AA8u},   // Gujarati ka .. qa
    {0x0AAAu, 0x0AB0u},   // Gujarati pa .. ra
    {0x0AB2u, 0x0AB3u},   // Gujarati la .. lla
    {0x0AB5u, 0x0AB9u},   // Gujarati sha .. ha
    {0x0AF9u, 0x0AF9u},   // Gujarati om
    {0x0B15u, 0x0B28u},   // Oriya ka .. na
    {0x0B2Au, 0x0B30u},   // Oriya pa .. ra
    {0x0B32u, 0x0B33u},   // Oriya la .. lla
    {0x0B35u, 0x0B39u},   // Oriya sha .. ha
    {0x0B5Cu, 0x0B5Du},   // Oriya vocalic l
    {0x0B71u, 0x0B71u},   // Oriya wa
    {0x0C15u, 0x0C28u},   // Telugu ka .. na
    {0x0C2Au, 0x0C39u},   // Telugu pa .. ha
    {0x0C58u, 0x0C5Au},   // Telugu vocalic l
    {0x0C95u, 0x0CA8u},   // Kannada ka .. qa
    {0x0CAAu, 0x0CB3u},   // Kannada pa .. lla
    {0x0CB5u, 0x0CB9u},   // Kannada sha .. ha
    {0x0CDEu, 0x0CDEu},   // Kannada length mark
    {0x0D04u, 0x0D04u},   // Malayalam letter vedan
    {0x0D15u, 0x0D3Au},   // Malayalam ka .. tta
    {0x0D3Eu, 0x0D3Eu},   // Malayalam letter dot reph
};

// Is this code point an Indic LINKER (a virama)?
[[nodiscard]] constexpr bool is_indic_linker(char32_t cp) noexcept {
    for (const WidthRange& r : kIndicLinker) {
        if (cp >= r.lo && cp <= r.hi) return true;
    }
    return false;
}

// Is this code point an Indic CONSONANT (a cluster that may start an
// Indic conjunct)?
[[nodiscard]] constexpr bool is_indic_consonant(char32_t cp) noexcept {
    for (const WidthRange& r : kIndicConsonant) {
        if (cp >= r.lo && cp <= r.hi) return true;
    }
    return false;
}

// ---- the SPACING MARK set (NEW TODAY) --------------------------------
//
// UAX #29's GB9a:  x SpacingMark
//
// so a SPACING MARK joins the PRECEDING cluster -- and, measured
// against the linked stdlib, it BREAKS the conjunct's left context:
// "ka" + spacing mark + virama + "ssa" is TWO clusters, not one.
// RESTRICTED: the marks the battery needs.
constexpr WidthRange kSpacingMark[] = {
    {0x0901u, 0x0903u},   // Devanagari candrabindu .. visarga
    {0x093Bu, 0x093Bu},   // Devanagari sign visarga
    {0x093Eu, 0x0940u},   // Devanagari vowel signs
    {0x0949u, 0x094Cu},   // Devanagari vowel signs
    {0x094Eu, 0x094Fu},   // Devanagari vowel signs
    {0x0982u, 0x0983u},   // Bengali anusvara .. visarga
    {0x09BEu, 0x09C0u},   // Bengali vowel signs
    {0x09C7u, 0x09C8u},   // Bengali vowel signs
    {0x09CBu, 0x09CCu},   // Bengali vowel signs
    {0x0A03u, 0x0A03u},   // Gujarati sign visarga
    {0x0A3Eu, 0x0A40u},   // Gujarati vowel signs
    {0x0A81u, 0x0A83u},   // Gujarati candrabindu .. visarga
};

// Is this code point a SpacingMark?
[[nodiscard]] constexpr bool is_spacing_mark(char32_t cp) noexcept {
    for (const WidthRange& r : kSpacingMark) {
        if (cp >= r.lo && cp <= r.hi) return true;
    }
    return false;
}

// ---- the HANGUL classes (NEW TODAY) ---------------------------------
//
// UAX #29's GB6, GB7 and GB8: the Hangul syllable sequences.
//
//   GB6: L x (L | V | LV | LVT)
//   GB7: (LV | V) x (V | T)
//   GB8: (LVT | T) x T
//
// so a sequence of Hangul jamo composes into ONE cluster -- and since
// a cluster is worth the width of its FIRST code point, "L V T" is
// worth the width of the L alone.
enum class HangulClass : unsigned char {
    None,
    L,
    V,
    T,
    LV,
    LVT
};

// The Hangul class of a code point.
[[nodiscard]] constexpr HangulClass hangul_class(char32_t cp) noexcept {
    if (cp >= 0x1100u && cp <= 0x115Fu) return HangulClass::L;
    if (cp >= 0xA960u && cp <= 0xA97Cu) return HangulClass::L;
    if (cp >= 0x1160u && cp <= 0x11A7u) return HangulClass::V;
    if (cp >= 0xD7B0u && cp <= 0xD7C6u) return HangulClass::V;
    if (cp >= 0x11A8u && cp <= 0x11FFu) return HangulClass::T;
    if (cp >= 0xD7CBu && cp <= 0xD7FBu) return HangulClass::T;
    if (cp >= 0xAC00u && cp <= 0xD7A3u) {
        // The precomposed syllables: every 28th one is LV, the rest LVT.
        return ((cp - 0xAC00u) % 28u == 0u) ? HangulClass::LV
                                            : HangulClass::LVT;
    }
    return HangulClass::None;
}

// Do these two Hangul classes stay in ONE cluster?
[[nodiscard]] constexpr bool hangul_joins(HangulClass prev,
                                           HangulClass cur) noexcept {
    if (cur == HangulClass::None || prev == HangulClass::None) {
        return false;
    }
    // GB6: L x (L | V | LV | LVT)
    if (prev == HangulClass::L) {
        return cur == HangulClass::L || cur == HangulClass::V ||
               cur == HangulClass::LV || cur == HangulClass::LVT;
    }
    // GB7: (LV | V) x (V | T)
    if (prev == HangulClass::LV || prev == HangulClass::V) {
        return cur == HangulClass::V || cur == HangulClass::T;
    }
    // GB8: (LVT | T) x T
    if (prev == HangulClass::LVT || prev == HangulClass::T) {
        return cur == HangulClass::T;
    }
    return false;
}

// ---- the RULES MASK (NEW TODAY) ---------------------------------------
//
// The UAX #29 rules the ONE column rule implements, as a mask, so that
// Section 17 can MEASURE what each rule is worth.  The arc's own rule
// is the default: every bit on.
//
//   gb9a  x SpacingMark
//   gb9b  Prepend x
//   gb9c  the Indic conjunct
//   gb11  EP Extend* ZWJ x EP
//   gb12  the regional-indicator pairs
//   gb68  the Hangul syllable sequences
//
// With `gb11` OFF the model reproduces Nov 17's DELETED rule exactly --
// the code point after a ZWJ joins unconditionally -- so the defect can
// be measured without keeping the defective function.
namespace gb {
inline constexpr unsigned gb9a = 1u;
inline constexpr unsigned gb9b = 2u;
inline constexpr unsigned gb9c = 4u;
inline constexpr unsigned gb11 = 8u;
inline constexpr unsigned gb12 = 16u;
inline constexpr unsigned gb68 = 32u;
// NEW TODAY -- the two things the collision battery measures:
//
//   gb9c_first   GB9c is consulted BEFORE GB11.  UAX #29's rules are
//                an ORDERED list, and GB9c is listed before GB11, so
//                at a boundary where both could apply, GB9c decides.
//                With the bit OFF, the conjunct is consulted AFTER
//                GB11 -- Nov 19's order, which is wrong.
//   gb9c_consume the conjunct CONSUMES its context: GB9c's left
//                context is "Consonant [Extend Linker]* Linker
//                [Extend Linker]*", so once the conjunct has formed
//                the Linker is SPENT, and a following Consonant needs
//                a Linker of its own.  With the bit OFF, the Linker is
//                kept -- Nov 19's rule, which is also wrong.
//
// The two bits are separate defects, so each can be MEASURED on its
// own: the order fixes the ZWJ cells, the consumption fixes the
// second-consonant cell.
inline constexpr unsigned gb9c_first = 64u;
inline constexpr unsigned gb9c_consume = 128u;

// NEW TODAY -- the CONTROL rules, and the ORDER they are
// consulted in.
//
//   gb3             GB3: CR x LF -- do not break between a CR
//                   and an LF.
//   gb4             GB4: (Control | CR | LF) v -- break AFTER a
//                   control.
//   gb5             GB5: v (Control | CR | LF) -- break BEFORE a
//                   control.
//   gb_control_first
//                   the control rules are consulted BEFORE GB9.
//                   UAX #29 lists GB3/GB4/GB5 before GB6..GB13,
//                   so an Extend after a NUL does NOT join it.
//                   With the bit OFF the control rules are
//                   consulted AFTER GB9 -- which is what the arc's
//                   rule did, and it is wrong.
//
// And ONE bit that is not a UAX #29 rule at all:
//
//   ascii_fast      libc++ 21's ASCII FAST PATH: a string that
//                   BEGINS with ASCII code units is charged one
//                   column per CODE UNIT for that prefix, and
//                   the cluster algorithm never runs over it.
//                   The bit is in the mask because the arc's
//                   ONE rule models the LINKED STDLIB, not the
//                   standard.
inline constexpr unsigned gb3 = 256u;
inline constexpr unsigned gb4 = 512u;
inline constexpr unsigned gb5 = 1024u;
inline constexpr unsigned gb_control_first = 2048u;
inline constexpr unsigned ascii_fast = 4096u;
inline constexpr unsigned all = gb9a | gb9b | gb9c | gb11 | gb12 | gb68 |
                                gb9c_first | gb9c_consume | gb3 | gb4 |
                                gb5 | gb_control_first | ascii_fast;
}  // namespace gb

// ---- TABLE: the CONTROL characters (NEW TODAY) ---------------------------
//
// UAX #29's GB3, GB4 and GB5 are about three properties of the
// grapheme-break table: Control, CR and LF.  The ranges below are read
// out of the LINKED STDLIB's own table
// (__format/extended_grapheme_cluster_table.h), and they are COMPLETE
// for those three properties -- 21 ranges, where the arc's other
// tables are RESTRICTED to what the batteries need.
enum class CtrlClass : unsigned char { none, control, cr, lf };

struct CtrlRange {
    char32_t lo;
    char32_t hi;
    CtrlClass cls;
};

constexpr CtrlRange kControlRanges[] = {
    {0x0000u, 0x0009u, CtrlClass::control},   // the C0 controls
    {0x000Au, 0x000Au, CtrlClass::lf},         // LINE FEED
    {0x000Bu, 0x000Cu, CtrlClass::control},
    {0x000Du, 0x000Du, CtrlClass::cr},         // CARRIAGE RETURN
    {0x000Eu, 0x001Fu, CtrlClass::control},
    {0x007Fu, 0x009Fu, CtrlClass::control},   // DEL and the C1 controls
    {0x00ADu, 0x00ADu, CtrlClass::control},   // SOFT HYPHEN
    {0x061Cu, 0x061Cu, CtrlClass::control},   // ARABIC LETTER MARK
    {0x180Eu, 0x180Eu, CtrlClass::control},   // MONGOLIAN VOWEL SEPARATOR
    {0x200Bu, 0x200Bu, CtrlClass::control},   // ZERO WIDTH SPACE
    {0x200Eu, 0x200Fu, CtrlClass::control},   // LRM, RLM
    {0x2028u, 0x202Eu, CtrlClass::control},   // LS, PS, LRE .. RLO
    {0x2060u, 0x206Fu, CtrlClass::control},   // WORD JOINER .. the invisible operators
    {0xFEFFu, 0xFEFFu, CtrlClass::control},   // ZERO WIDTH NO-BREAK SPACE
    {0xFFF0u, 0xFFFBu, CtrlClass::control},   // the specials
    {0x13430u, 0x1343Fu, CtrlClass::control},  // EGYPTIAN HIEROGLYPH CONTROLS
    {0x1BCA0u, 0x1BCA3u, CtrlClass::control},  // SHORTHAND FORMAT CONTROLS
    {0x1D173u, 0x1D17Au, CtrlClass::control},  // MUSICAL SYMBOL CONTROLS
    {0xE0000u, 0xE001Fu, CtrlClass::control},  // the language tags
    {0xE0080u, 0xE00FFu, CtrlClass::control},  // the supplement
    {0xE01F0u, 0xE0FFFu, CtrlClass::control},  // the tags and variation selectors
};

[[nodiscard]] constexpr CtrlClass ctrl_class(char32_t cp) noexcept {
    for (const CtrlRange& r : kControlRanges) {
        if (cp >= r.lo && cp <= r.hi) return r.cls;
    }
    return CtrlClass::none;
}

// What the CONTROL rules say about the boundary between the cluster's
// LAST code point and the code point that follows it.
//
//   none   no control rule applies
//   brk    the cluster ends here (GB4 or GB5)
//   join   the code point JOINS the cluster (GB3: CR x LF)
enum class CtrlAction : unsigned char { none, brk, join };

[[nodiscard]] constexpr CtrlAction control_action(char32_t prev_cp,
                                                  char32_t cp,
                                                  unsigned rules) noexcept {
    const CtrlClass prev = ctrl_class(prev_cp);
    const CtrlClass cur = ctrl_class(cp);
    // GB3 comes FIRST, so it beats GB4's "break after a CR".
    if ((rules & gb::gb3) != 0u && prev == CtrlClass::cr &&
        cur == CtrlClass::lf)
        return CtrlAction::join;
    // GB4: break AFTER a control.
    if ((rules & gb::gb4) != 0u && prev != CtrlClass::none)
        return CtrlAction::brk;
    // GB5: break BEFORE a control.
    if ((rules & gb::gb5) != 0u && cur != CtrlClass::none)
        return CtrlAction::brk;
    return CtrlAction::none;
}

// Is this byte an ASCII code unit?  libc++'s fast path tests the CODE
// UNIT, not the code point.
[[nodiscard]] constexpr bool is_ascii_byte(char c) noexcept {
    return static_cast<unsigned char>(c) < 0x80u;
}

// One extended grapheme cluster: its length in BYTES and its width in
// COLUMNS.
struct Cluster {
    std::size_t bytes;
    std::size_t columns;
};

// Split `s` into its extended grapheme clusters, under UAX #29's GB6,
// GB7 and GB8 (the Hangul syllable sequences), GB9 (an Extend code
// point, or a ZWJ, joins the PRECEDING cluster), GB9a (a SpacingMark
// joins the preceding cluster too), GB9b (a Prepend joins the
// FOLLOWING cluster), GB9c (the Indic conjunct), GB11 (a pictographic
// joins the preceding cluster when it follows a pictographic Extend*
// ZWJ) and GB12/GB13 (the regional indicators pair up).
//
// A cluster is worth the width of its FIRST code point.
[[nodiscard]] std::vector<Cluster> split_clusters(std::string_view s,
                                                  unsigned rules) {
    std::vector<Cluster> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const std::size_t start = i;
        const Utf8CodePoint first = decode_utf8(s.substr(i));
        if (first.length == 0) break;
        i += first.length;
        const std::size_t columns = is_width_two(first.value) ? 2u : 1u;

        // The cluster's LAST code point -- the one the boundary
        // rules look back at.  It is the previous iteration's code
        // point, because reaching the next iteration means the
        // previous one was consumed.
        char32_t last_cp = first.value;

        // The cluster's state, for the rules that look BACK.
        bool ep_run = is_extended_pictographic(first.value);
        bool last_was_zwj = false;
        bool ep_before_zwj = false;
        bool prepend_pending = ((rules & gb::gb9b) != 0u) &&
                              is_prepend(first.value);
        bool cb_consonant = ((rules & gb::gb9c) != 0u) &&
                            is_indic_consonant(first.value);
        bool cb_linker = false;
        HangulClass hangul_prev = ((rules & gb::gb68) != 0u)
                                       ? hangul_class(first.value)
                                       : HangulClass::None;
        // GB12/GB13: a run of regional indicators pairs up, so the
        // cluster boundary depends on the PARITY of the run.  The count
        // is of the CONTIGUOUS run of RIs ending at the previous code
        // point, so it resets on anything else.
        std::size_t ri_run = is_regional_indicator(first.value) ? 1u : 0u;

        while (i < s.size()) {
            const Utf8CodePoint cp = decode_utf8(s.substr(i));
            if (cp.length == 0) break;

            // The cluster's last code point -- the one the boundary rules
            // look back at.
            const char32_t prev_cp = last_cp;
            last_cp = cp.value;

            // GB3/GB4/GB5 -- the CONTROL rules, consulted FIRST, which is
            // UAX #29's order: GB3, GB4 and GB5 come before GB6..GB13,
            // so an Extend after a NUL does NOT join the NUL's cluster,
            // and a Prepend does not swallow a control.
            if ((rules & gb::gb_control_first) != 0u) {
                const CtrlAction act =
                    control_action(prev_cp, cp.value, rules);
                if (act == CtrlAction::brk) break;
                if (act == CtrlAction::join) {
                    // GB3: the LF joins the CR's cluster.  The cluster
                    // ends right after it -- GB4 breaks after a LF.
                    i += cp.length;
                    last_was_zwj = false;
                    prepend_pending = false;
                    ri_run = 0;
                    hangul_prev = HangulClass::None;
                    continue;
                }
            }

            // GB9c's LINKER: a virama joins the cluster, and marks the
            // cluster as "a consonant with a linker in it".
            if ((rules & gb::gb9c) != 0u && is_indic_linker(cp.value)) {
                cb_linker = true;
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB9: an Extend joins the preceding cluster.
            if (is_cluster_extend(cp.value)) {
                i += cp.length;
                last_was_zwj = false;
                prepend_pending = false;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }

            // The same CONTROL rules, but consulted AFTER GB9 -- the
            // order the arc's rule used.  With `gb_control_first` OFF
            // this is the one that runs, and it is what lets an Extend
            // after a NUL join the NUL's cluster.
            if ((rules & gb::gb_control_first) == 0u) {
                const CtrlAction act =
                    control_action(prev_cp, cp.value, rules);
                if (act == CtrlAction::brk) break;
                if (act == CtrlAction::join) {
                    i += cp.length;
                    last_was_zwj = false;
                    prepend_pending = false;
                    ri_run = 0;
                    hangul_prev = HangulClass::None;
                    continue;
                }
            }
            // GB9a: a SpacingMark joins the preceding cluster -- and it
            // BREAKS the conjunct's left context, which is why "ka" +
            // spacing mark + virama + "ssa" is TWO clusters.
            if ((rules & gb::gb9a) != 0u && is_spacing_mark(cp.value)) {
                cb_consonant = false;
                cb_linker = false;
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB9c: the conjunct -- and it comes BEFORE GB11, because
            // UAX #29's rules are an ORDERED list and GB9c is listed
            // before GB11.  GB9c's left context is "Consonant [Extend
            // Linker]* Linker [Extend Linker]*", and a ZWJ is an
            // Extend, so a conjunct SURVIVES a ZWJ: "ka" virama ZWJ
            // "ssa" is ONE cluster.  Nov 19's rule asked GB11 first,
            // and cut the conjunct in two.
            if ((rules & gb::gb9c) != 0u &&
                (rules & gb::gb9c_first) != 0u && cb_consonant &&
                cb_linker && is_indic_consonant(cp.value)) {
                // The conjunct forms, and it SPENDS its context: the
                // Linker is used up, so a following Consonant needs a
                // Linker of its own.
                if ((rules & gb::gb9c_consume) != 0u) cb_linker = false;
                last_was_zwj = false;   // the ZWJ's pending check is spent
                ep_run = false;
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB9: a ZWJ joins the preceding cluster.
            if (cp.value == kZwj) {
                last_was_zwj = true;
                ep_before_zwj = ep_run;
                ep_run = false;
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB11: the code point after a ZWJ.
            if (last_was_zwj) {
                if ((rules & gb::gb11) != 0u &&
                    !(ep_before_zwj && is_extended_pictographic(cp.value))) {
                    break;   // GB11's condition fails: the cluster ends
                }
                ep_run = is_extended_pictographic(cp.value);
                last_was_zwj = false;
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB6/GB7/GB8: the Hangul syllable sequences.
            if ((rules & gb::gb68) != 0u) {
                const HangulClass cur = hangul_class(cp.value);
                if (cur != HangulClass::None) {
                    if (!hangul_joins(hangul_prev, cur)) break;
                    hangul_prev = cur;
                    i += cp.length;
                    ri_run = 0;
                    continue;
                }
            }
            // GB9b: a Prepend joins the FOLLOWING cluster.
            if (prepend_pending) {
                prepend_pending = is_prepend(cp.value);
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB9c: the consonant after a linker -- the LATE check, for
            // the OLD order only (Nov 19's rule): GB11 was asked first,
            // so a conjunct after a ZWJ never got its chance to form.
            if ((rules & gb::gb9c) != 0u &&
                (rules & gb::gb9c_first) == 0u && cb_consonant &&
                cb_linker && is_indic_consonant(cp.value)) {
                if ((rules & gb::gb9c_consume) != 0u) cb_linker = false;
                i += cp.length;
                ri_run = 0;
                hangul_prev = HangulClass::None;
                continue;
            }
            // GB12/GB13: the regional indicators pair up, so the
            // boundary between two of them depends on the parity of the
            // run: an RI joins the preceding one when the run so far is
            // ODD, and starts a new cluster when it is EVEN.
            if ((rules & gb::gb12) != 0u && is_regional_indicator(cp.value)) {
                if (ri_run % 2u == 1u) {
                    ++ri_run;
                    i += cp.length;
                    hangul_prev = HangulClass::None;
                    continue;
                }
                break;   // the run's parity says: a NEW cluster
            }
            break;
        }
        out.push_back(Cluster{i - start, columns});
    }
    return out;
}

// The columns of `s` under the CLUSTER rules alone -- no ASCII fast
// path.  This is the model the fast path is measured against.
[[nodiscard]] std::size_t cluster_columns(std::string_view s, unsigned rules) {
    std::size_t columns = 0;
    for (const Cluster& cl : split_clusters(s, rules)) columns += cl.columns;
    return columns;
}

// The ONE column rule: the width of a string in COLUMNS.
//
// This is the arc's only column rule.  Nov 17's defective rule -- the
// one that joined the code point after a ZWJ unconditionally -- is
// DELETED; the `rules` mask exists so Section 17 can MEASURE what it
// was worth, not to run it.
//
// NEW TODAY: libc++ 21 has an ASCII FAST PATH, and it is part of what
// the LINKED STDLIB does.  A string that BEGINS with ASCII code units
// is charged one column per CODE UNIT for that prefix -- the cluster
// algorithm never runs over it -- except that the LAST ASCII code unit
// before the first non-ASCII one is handed back, because it might join
// what follows.  The only cluster the ASCII rules can form is GB3's
// CR x LF, so the fast path is visible exactly there: a CR LF pair is
// TWO columns at the start of an ASCII string and ONE column in the
// middle of a non-ASCII-leading one.
[[nodiscard]] std::size_t count_columns(std::string_view s,
                                        unsigned rules = gb::all) {
    if ((rules & gb::ascii_fast) != 0u && !s.empty() && is_ascii_byte(s[0])) {
        std::size_t k = 0;
        while (k < s.size() && is_ascii_byte(s[k])) ++k;
        // All ASCII: one column per code unit, and no cluster at all.
        if (k == s.size()) return k;
        // The last ASCII code unit is handed back to the cluster rules.
        const std::size_t head = k - 1u;
        return head + cluster_columns(s.substr(head), rules);
    }
    return cluster_columns(s, rules);
}

// The BYTE length of the longest prefix of `s` whose field width is no
// greater than `budget`, cut at a cluster boundary.  This is the
// standard's rule, in the arc's own words.
//
// NEW TODAY: the ASCII FAST PATH cuts at a CODE UNIT, not at a cluster
// boundary -- which is why the stdlib's precision can cut a CR LF pair
// IN HALF.
[[nodiscard]] std::size_t prefix_bytes_for_precision(std::string_view s,
                                                     std::size_t budget,
                                                     unsigned rules = gb::all) {
    if ((rules & gb::ascii_fast) != 0u && !s.empty() && budget != 0u &&
        is_ascii_byte(s[0])) {
        std::size_t it = 0;
        std::size_t max = budget;
        for (;;) {
            --max;
            ++it;
            if (it == s.size()) return s.size();
            if (max == 0) {
                if (is_ascii_byte(s[it])) return it;
                break;
            }
            if (!is_ascii_byte(s[it])) break;
        }
        --it;
        ++max;
        const std::size_t ascii_size = it;
        std::size_t columns = 0;
        std::size_t bytes = ascii_size;
        for (const Cluster& cl : split_clusters(s.substr(ascii_size), rules)) {
            if (columns + cl.columns > max) break;
            columns += cl.columns;
            bytes += cl.bytes;
        }
        return bytes;
    }
    std::size_t columns = 0;
    std::size_t bytes = 0;
    for (const Cluster& cl : split_clusters(s, rules)) {
        if (columns + cl.columns > budget) break;
        columns += cl.columns;
        bytes += cl.bytes;
    }
    return bytes;
}

// The width the LINKED stdlib charges for `arg`, MEASURED: let it pad
// the argument to a known width with a fill that cannot occur in the
// argument, and count the fill characters it wrote.
[[nodiscard]] long charged_width(const std::string& arg, char fill = '~') {
    const std::string fmt = "{:" + std::string(1, fill) + ">40s}";
    const std::string out = std::vformat(fmt, std::make_format_args(arg));
    std::size_t pad = 0;
    while (pad < out.size() && out[pad] == fill) ++pad;
    return 40L - static_cast<long>(pad);
}

// Count the U+FFFD characters in a string.
[[nodiscard]] std::size_t count_replacement_chars(const std::string& s) {
    std::size_t n = 0;
    for (std::size_t i = 0; i + 2 < s.size(); ++i) {
        if (static_cast<unsigned char>(s[i]) == 0xEFu &&
            static_cast<unsigned char>(s[i + 1]) == 0xBFu &&
            static_cast<unsigned char>(s[i + 2]) == 0xBDu) {
            ++n;
            i += 2;
        }
    }
    return n;
}

void section15(counters& c) {
    section("Section 15 — the FIELD WIDTH");

    const ImplPolicy& linked = linked_policy();

    // ---- the battery ---------------------------------------------------
    //
    // One row per kind of argument the width rule has to cope with:
    // ASCII, narrow multi-byte, wide multi-byte, the standard's three
    // explicit override bands, combining marks, variation selectors and
    // ZWJ sequences.
    struct Arg {
        const char* label;
        std::string bytes;
    };
    const std::vector<Arg> args = {
        {"ascii 'abc'",              "abc"},
        {"U+00E9 e-acute (N)",       "\xC3\xA9"},
        {"U+20AC euro (N)",          "\xE2\x82\xAC"},
        {"U+1F600 grinning (W)",     "\xF0\x9F\x98\x80"},
        {"U+1F600 + FE0F",           "\xF0\x9F\x98\x80\xEF\xB8\x8F"},
        {"U+1F600 + FE0F + FE0F",    "\xF0\x9F\x98\x80\xEF\xB8\x8F\xEF\xB8\x8F"},
        {"U+1F600+FE0F+U+1F600",     "\xF0\x9F\x98\x80\xEF\xB8\x8F\xF0\x9F\x98\x80"},
        {"U+1F600 + FE0E (VS15)",    "\xF0\x9F\x98\x80\xEF\xB8\x8E"},
        {"U+1F600 + ZWJ + U+1F600",  "\xF0\x9F\x98\x80\xE2\x80\x8D\xF0\x9F\x98\x80"},
        {"U+20AC + FE0F",            "\xE2\x82\xAC\xEF\xB8\x8F"},
        {"U+4E00 + FE0F",            "\xE4\xB8\x80\xEF\xB8\x8F"},
        {"'a' + FE0F",               "a\xEF\xB8\x8F"},
        {"FE0F alone",               "\xEF\xB8\x8F"},
        {"U+4E00 CJK (W)",           "\xE4\xB8\x80"},
        {"U+FF21 fullwidth A (F)",   "\xEF\xBC\xA1"},
        {"U+FFFD replacement (A)",   "\xEF\xBF\xBD"},
        {"U+4DC1 Yijing (/12)",      "\xE4\xB7\x81"},
        {"U+0301 combining acute",   "\xCC\x81"},
        {"'e' + U+0301",             "e\xCC\x81"},
        {"'a' + 2 combining marks",  "a\xCC\x81\xCC\x82"},
        {"family ZWJ (7 cp)",
         "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
         "\xF0\x9F\x91\xA6\xE2\x80\x8D\xF0\x9F\x91\xA7"},
        {"two CJK",                  "\xE4\xB8\x80\xE4\xB8\x80"},
        {"U+1F300 (/12)",            "\xF0\x9F\x8C\x80"},
        {"U+1F900 (/12)",            "\xF0\x9F\xA4\x80"},
        {"U+2764 heart (N)",         "\xE2\x9D\xA4"},
        {"U+2764 + FE0F",            "\xE2\x9D\xA4\xEF\xB8\x8F"},
        {"U+1F441 eye (W)",          "\xF0\x9F\x91\xA1"},
        {"U+1F441 + FE0F",           "\xF0\x9F\x91\xA1\xEF\xB8\x8F"},
    };

    // ---- 15.1 the census -----------------------------------------------
    //
    // Three candidate units, one battery, one verdict each.  The stdlib's
    // own charged width is the oracle: it is measured, not asserted.
    std::size_t bytes_agree = 0;
    std::size_t cps_agree = 0;
    std::size_t cols_agree = 0;

    std::printf("  the census -- bytes vs code points vs COLUMNS:\n");
    std::printf("    %-28s %5s %5s %5s %6s %6s\n", "argument", "bytes",
                "cps", "cols", "model", "stdlib");

    for (const Arg& a : args) {
        const std::size_t nbytes = a.bytes.size();
        const std::size_t ncp = count_code_points(a.bytes);
        const std::size_t ncols = count_columns(a.bytes);
        const long charged = charged_width(a.bytes);

        if (static_cast<long>(nbytes) == charged) ++bytes_agree;
        if (static_cast<long>(ncp) == charged) ++cps_agree;
        if (static_cast<long>(ncols) == charged) ++cols_agree;

        std::printf("    %-28s %5zu %5zu %5zu %6zu %6ld\n", a.label, nbytes,
                    ncp, ncols, ncols, charged);
    }

    std::printf("\n    the units, over %zu cells:\n", args.size());
    std::printf("      bytes       agree on %2zu of %zu cells\n", bytes_agree,
                args.size());
    std::printf("      code points agree on %2zu of %zu cells\n", cps_agree,
                args.size());
    std::printf("      COLUMNS     agree on %2zu of %zu cells\n", cols_agree,
                args.size());

    c.expect_eq(bytes_agree, 1u, "only 'abc' has as many bytes as columns");
    c.expect_eq(cps_agree, 11u,
                "the code-point count agrees on 11 of the 28 cells");
    c.expect_eq(cols_agree, args.size(),
                "the column count agrees on every cell");

    // The euro -- Nov 13's whole battery -- is one column per code point,
    // which is why the code-point reading survived a whole lesson.
    const std::string three_euros =
        "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC";
    c.expect_eq(count_columns(three_euros), 3u,
                "three euros are three columns");
    c.expect_eq(count_code_points(three_euros), 3u,
                "three euros are three code points");
    std::printf("\n    Nov 13's battery was three EUROS: one column each, so\n"
                "    the code-point reading and the column reading could not\n"
                "    be told apart.  The euro is what hid the rule.\n");

    // ---- 15.2 the fill is assumed to be ONE column wide ----------------
    //
    // [format.string.std]'s note says so: "Since fill characters are
    // assumed to have a field width of 1, use of a character with a
    // different field width can produce misaligned output."
    //
    // So the FILL's own width does not enter the padding count -- and a
    // two-column fill pads the field WIDER than the width asked for.
    struct FillArg {
        const char* label;
        std::string bytes;
        std::size_t expected_cols;   // the field's REAL width in columns
    };
    const std::vector<FillArg> fills = {
        {"ascii '*'",     "*",                     10},
        {"U+1F600 (W)",   "\xF0\x9F\x98\x80",      17},
        {"U+FF21 (F)",    "\xEF\xBC\xA1",           17},
    };
    const std::string filler = "abc";
    const std::size_t filler_cols = count_columns(filler);

    std::printf("\n  the fill is assumed to be ONE column wide"
                " (width 10, argument \"abc\"):\n");
    for (const FillArg& f : fills) {
        const std::string fmt = "{:" + f.bytes + "<10s}";
        const std::string out =
            std::vformat(fmt, std::make_format_args(filler));
        const std::size_t reps = (out.size() - filler.size()) / f.bytes.size();
        const std::size_t fill_cols = count_columns(f.bytes);
        const std::size_t actual = filler_cols + reps * fill_cols;
        std::printf("    %-14s %zu repetitions x %zu column(s) each -> the"
                    " field is %zu columns wide (the width asked for was 10)\n",
                    f.label, reps, fill_cols, actual);
        c.expect_eq(reps, 7u, "the padding count does not see the fill's width");
        c.expect_eq(actual, f.expected_cols, "the field's real width");
    }
    c.expect_eq(count_columns("\xEF\xBF\xBD"), 1u,
                "U+FFFD is one column wide");

    // ---- 15.3 the recovery's substitution count -----------------------
    //
    // Nov 16's chain asked how many U+FFFDs a recovery substitutes.
    // MEASURED: libstdc++ 15 substitutes ONE, for the whole maximal
    // subpart -- the two-byte subpart "\xE2\x82" yields one U+FFFD per
    // repetition, not two.  libc++ 21 never recovers at all.
    {
        const bool libcxx = std::strcmp(linked.name, "libc++ 21") == 0;
        struct Rec {
            const char* label;
            std::string bytes;
        };
        const std::vector<Rec> recs = {
            {"invalid-lead \\x80",   "\x80"},
            {"truncated \\xE2\\x82", "\xE2\x82"},
        };
        std::printf("\n  the recovery's substitution count"
                    " (width 10, argument \"abc\"):\n");
        for (const Rec& r : recs) {
            const std::string fmt = "{:" + r.bytes + "<10s}";
            std::printf("    %-22s ", r.label);
            try {
                const std::string out =
                    std::vformat(fmt, std::make_format_args(filler));
                const std::size_t nfffd = count_replacement_chars(out);
                const std::size_t reps =
                    (out.size() - filler.size()) / 3u;
                std::printf("ACCEPT: %zu U+FFFD over %zu repetitions"
                            " -> %zu per repetition\n",
                            nfffd, reps,
                            static_cast<std::size_t>(reps == 0 ? 0u : nfffd / reps));
                if (!libcxx) {
                    c.expect_eq(nfffd, reps,
                                "one U+FFFD per repetition, whatever the"
                                " subpart's length");
                    c.expect_eq(reps, 7u, "the recovered fill pads seven times");
                }
            } catch (const std::format_error& e) {
                std::printf("REJECT: %s\n", e.what());
                if (libcxx) {
                    c.check(true);   // libc++ 21 never recovers
                } else {
                    c.check(false);
                }
            }
        }
        if (libcxx) {
            std::printf("    libc++ 21 makes every ill-formed fill fatal, so"
                        " there is no\n    substitution to count.\n");
        }
    }

    // ---- 15.4 the linked stdlib, on the same battery -------------------
    //
    // The model must agree with the LINKED stdlib on every cell -- under
    // libc++ 21 AND under libstdc++ 15, with no source change between
    // them.  The restricted table is the thing under test: if it were
    // wrong for any cell of the battery, this is where it shows.
    {
        std::size_t agree = 0;
        std::vector<std::string> bad;
        for (const Arg& a : args) {
            const long charged = charged_width(a.bytes);
            const std::size_t model = count_columns(a.bytes);
            if (static_cast<long>(model) == charged) {
                ++agree;
            } else if (bad.size() < 6) {
                bad.push_back("    " + std::string{a.label} + ": the model says " +
                              std::to_string(model) + ", the stdlib says " +
                              std::to_string(charged));
            }
        }
        std::printf("\n  the linked stdlib (\"%s\"): the model agrees on %zu"
                    " of %zu cells\n", linked.name, agree, args.size());
        for (const std::string& s : bad) std::printf("%s\n", s.c_str());
        c.expect_eq(agree, args.size(),
                    "the model agrees with the linked stdlib on the battery");
    }
}

// ============================================================================
// Section 16 — THE PRECISION.
//
// Nov 17 measured the WIDTH's unit: COLUMNS, and the cluster rule that
// decides which code points are free.  Nov 17's chain asked what the
// PRECISION's unit is.  Today answers it, and the answer is that the
// precision is the width's rule applied to a PREFIX.
//
// [format.string.std]/15 (C++26's wording; C++23's /13 says the same
// thing under a different paragraph number):
//
//   "The precision option is valid for floating-point and string types.
//    ...
//    For string types, this option specifies the longest prefix of the
//    formatted argument to be included in the replacement field such
//    that the field width of the prefix is no greater than the value
//    of this option."
//
// Three words carry the whole rule: "longest prefix", "field width",
// and "no greater than".  So:
//
//   - the precision's unit is the FIELD WIDTH, i.e. the COLUMN — the
//     same unit, and the same table, as the width option;
//   - the prefix is cut at a CLUSTER BOUNDARY, and a cluster is worth
//     the width of its FIRST code point;
//   - the precision applies to the ARGUMENT, before the width pads.
//
//   16.1  the census: BYTES vs CODE POINTS vs COLUMNS, over a battery
//         of (argument, precision) cells, scored against the linked
//         stdlib.
//   16.2  the truncation lands on a CLUSTER boundary.
//   16.3  ONE table, TWO rules: the width's table is the precision's
//         table.
//   16.4  the precision truncates BEFORE the width pads.
//   16.5  the ESCAPED type's precision counts the columns of the
//         ESCAPED output, and can cut an escape sequence in half.
//   16.6  THE DEFECT, AND THE RULE THAT FIXED IT: UAX #29's GB11, and
//         the arc's ZWJ rule that joined unconditionally.
//   16.7  the ONE rule against the linked stdlib, on the width.
// ============================================================================
void section16(counters& c) {
    section("Section 16 — the PRECISION");

    const ImplPolicy& linked = linked_policy();

    // ---- the battery ---------------------------------------------------
    //
    // One row per kind of argument the precision has to cope with: ASCII,
    // narrow multi-byte, wide multi-byte, the standard's three explicit
    // override bands, a combining mark, a ZWJ sequence, and the three
    // cells where UAX #29's GB11 decides the answer.
    struct Arg {
        const char* label;
        std::string bytes;
    };
    const std::vector<Arg> args = {
        {"ascii 'abc'",              "abc"},
        {"U+00E9 e-acute (N)",       "\xC3\xA9"},
        {"U+20AC euro (N)",          "\xE2\x82\xAC"},
        {"U+1F600 grinning (W)",     "\xF0\x9F\x98\x80"},
        {"U+4E00 CJK (W)",           "\xE4\xB8\x80"},
        {"U+FF21 fullwidth A (F)",   "\xEF\xBC\xA1"},
        {"U+4DC1 Yijing (/12)",      "\xE4\xB7\x81"},
        {"'e' + U+0301",             "e\xCC\x81"},
        {"family ZWJ (2 cp)",        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9"},
        {"'a' + U+1F600",            "a\xF0\x9F\x98\x80"},
        {"U+1F600 + 'a'",            "\xF0\x9F\x98\x80" "a"},
        {"'a' ZWJ U+1F600 (GB11)",   "a\xE2\x80\x8D\xF0\x9F\x98\x80"},
        {"U+2764 heart ZWJ woman",   "\xE2\x9D\xA4\xE2\x80\x8D\xF0\x9F\x91\xA9"},
        {"man ZWJ 'a' (GB11)",       "\xF0\x9F\x91\xA8\xE2\x80\x8D" "a"},
    };
    const std::vector<std::size_t> precisions = {0u, 1u, 2u, 3u};

    // ---- 16.1 the census -----------------------------------------------
    //
    // Three candidate truncation rules, one battery, one verdict each.
    // The stdlib's own output is the oracle: it is measured, not
    // asserted.
    //
    //   BYTES       keep the first `p` bytes.
    //   CODE POINTS keep the first `p` code points.
    //   COLUMNS     keep the longest prefix whose FIELD WIDTH is no
    //               greater than `p`, cut at a cluster boundary.
    std::size_t bytes_agree = 0;
    std::size_t cps_agree = 0;
    std::size_t cols_agree = 0;
    std::size_t cells = 0;

    std::printf("  the census — three candidate truncation rules:\n");
    std::printf("    %-26s %4s %6s %6s %6s\n", "argument", "prec", "bytes",
                "cps", "cols");

    for (const Arg& a : args) {
        for (const std::size_t p : precisions) {
            const std::string fmt = "{:." + std::to_string(p) + "}";
            const std::string actual =
                std::vformat(fmt, std::make_format_args(a.bytes));
            ++cells;

            // BYTES: the first `p` bytes.
            const std::size_t nbytes = (p < a.bytes.size()) ? p : a.bytes.size();
            const std::string by_bytes = a.bytes.substr(0, nbytes);

            // CODE POINTS: the first `p` code points.
            std::size_t ncps = 0;
            std::size_t ibytes = 0;
            while (ibytes < a.bytes.size() && ncps < p) {
                const Utf8CodePoint cp = decode_utf8(a.bytes.substr(ibytes));
                if (cp.length == 0) break;
                ibytes += cp.length;
                ++ncps;
            }
            const std::string by_cps = a.bytes.substr(0, ibytes);

            // COLUMNS: the longest prefix whose field width fits.
            const std::string by_cols = a.bytes.substr(
                0, prefix_bytes_for_precision(a.bytes, p));

            if (by_bytes == actual) ++bytes_agree;
            if (by_cps == actual) ++cps_agree;
            if (by_cols == actual) ++cols_agree;

            std::printf("    %-26s %4zu %6s %6s %6s\n", a.label, p,
                        by_bytes == actual ? "ok" : "-",
                        by_cps == actual ? "ok" : "-",
                        by_cols == actual ? "ok" : "-");
        }
    }

    std::printf("\n    the rules, over %zu cells:\n", cells);
    std::printf("      BYTES       agree on %2zu of %zu cells\n", bytes_agree,
                cells);
    std::printf("      CODE POINTS agree on %2zu of %zu cells\n", cps_agree,
                cells);
    std::printf("      COLUMNS     agree on %2zu of %zu cells\n", cols_agree,
                cells);

    c.expect_eq(cols_agree, cells,
                "the column rule reproduces the linked stdlib on every cell");
    c.expect_eq(cps_agree < cells, true,
                "the code-point rule loses at least one cell");
    c.expect_eq(bytes_agree < cells, true,
                "the byte rule loses at least one cell");

    // ---- 16.2 the truncation lands on a CLUSTER boundary ---------------
    //
    // 'e' plus U+0301 is ONE cluster: one column, two code points, three
    // bytes.  A precision of 1 keeps the WHOLE cluster -- the truncation
    // does not split it, and it does not fall back to a code-point count.
    {
        const std::string eacute("e\xCC\x81");
        const std::string out =
            std::vformat("{:.1}", std::make_format_args(eacute));
        std::printf("\n  the truncation lands on a CLUSTER boundary:\n");
        std::printf("    'e' + U+0301 is one cluster: 1 column, 2 code"
                    " points, 3 bytes\n");
        std::printf("    a precision of 1 keeps %zu bytes / %zu code points\n",
                    out.size(), count_code_points(out));
        c.expect_eq(out.size(), 3u,
                    "the whole cluster is kept, not one code point");
        c.expect_eq(count_code_points(out), 2u,
                    "the combining mark comes along with its base");
        c.expect_eq(count_columns(out), 1u,
                    "the kept cluster is one column wide");

        // And the wide cluster: a precision of 1 cannot fit a
        // two-column cluster, so NOTHING is kept -- the cluster is not
        // cut in half either.
        const std::string grin("\xF0\x9F\x98\x80");
        const std::string out2 =
            std::vformat("{:.1}", std::make_format_args(grin));
        std::printf("    a two-column cluster and a precision of 1 keeps"
                    " %zu bytes\n", out2.size());
        c.expect_eq(out2.size(), 0u,
                    "a cluster that does not fit is dropped whole");
    }

    // ---- 16.3 ONE table, TWO rules ------------------------------------
    //
    // The question Nov 17's chain asked: does the arc's scanner need a
    // SECOND Unicode table for the precision?  No: the width's table
    // IS the precision's table, because both rules are stated in field
    // width units.
    {
        const std::string grin("\xF0\x9F\x98\x80");
        std::printf("\n  ONE table, TWO rules:\n");
        std::printf("    U+1F600 is two columns wide, so a budget of 1 keeps"
                    " %zu bytes and a budget of 2 keeps %zu\n",
                    prefix_bytes_for_precision(grin, 1u),
                    prefix_bytes_for_precision(grin, 2u));
        c.expect_eq(prefix_bytes_for_precision(grin, 1u), 0u,
                    "a two-column cluster does not fit in a budget of 1");
        c.expect_eq(prefix_bytes_for_precision(grin, 2u), 4u,
                    "a two-column cluster fits exactly in a budget of 2");
        c.expect_eq(count_columns(grin), 2u,
                    "the width's table says the same thing");
    }

    // ---- 16.4 the precision truncates BEFORE the width pads ------------
    //
    // The precision applies to the ARGUMENT; the width then pads what
    // is left.  So the padding is computed from the TRUNCATED prefix.
    {
        const std::string ag("abc\xF0\x9F\x98\x80");
        const std::string out =
            std::vformat("{:>8.2}", std::make_format_args(ag));
        std::printf("\n  the precision truncates BEFORE the width pads:\n");
        std::printf("    \"{:>8.2}\" on \"abc\" + U+1F600 -> %zu bytes,"
                    " %zu columns\n", out.size(), count_columns(out));
        c.expect_eq(out.size(), 8u, "the field is eight bytes");
        c.expect_eq(count_columns(out), 8u, "the field is eight columns");
        c.check(out.substr(0, 6u) == "      ");
        c.check(out.substr(6u) == "ab");
    }

    // ---- 16.5 the ESCAPED type's precision ----------------------------
    //
    // The '?' type copies the ESCAPED string to the output, so its
    // precision counts the columns of the ESCAPED output -- and the cut
    // can land in the middle of an escape sequence.
    {
        const std::string q("a\"b");
        const std::string out =
            std::vformat("{:.3?}", std::make_format_args(q));
        std::printf("\n  the ESCAPED type's precision counts the columns of"
                    " the ESCAPED output:\n");
        std::printf("    \"{:.3?}\" on a\"b -> %zu bytes\n", out.size());
        c.expect_eq(out.size(), 3u, "three columns of the escaped output");
        c.check(out == "\"a\\");
    }

    // ---- 16.6 THE DEFECT, AND THE RULE THAT FIXED IT -----------------
    //
    // The arc's rule (Nov 17) said: the code point after a ZWJ joins
    // the cluster.  UAX #29's GB11 says: it joins only when the ZWJ is
    // preceded by Extended_Pictographic Extend* AND the code point
    // after it is Extended_Pictographic.
    //
    // TODAY the two rules are ONE (Section 17): the defective rule is
    // DELETED, and this section re-measures the two cells that exposed
    // the defect against the linked stdlib.  Nov 18's notes record the
    // deleted rule's answers -- 1 column and 2 columns -- where the
    // stdlib charges 3 and 3.
    {
        const std::string azwjgrin("a\xE2\x80\x8D\xF0\x9F\x98\x80");
        const long charged = charged_width(azwjgrin);
        const std::size_t model = count_columns(azwjgrin);
        std::printf("\n  THE DEFECT — UAX #29's GB11 (the rule is now ONE):\n");
        std::printf("    'a' ZWJ U+1F600: the stdlib charges %ld columns,"
                    " the ONE rule says %zu\n", charged, model);
        c.expect_eq(model, static_cast<std::size_t>(charged),
                    "the ONE rule agrees with the linked stdlib");
        c.expect_eq(model, 3u,
                    "three columns: 'a', the ZWJ's cluster, and the emoji");

        // The other direction: a ZWJ followed by a NON-pictographic does
        // not join either, so 'man' ZWJ 'a' is TWO clusters.
        const std::string manzwja("\xF0\x9F\x91\xA8\xE2\x80\x8D" "a");
        const long charged2 = charged_width(manzwja);
        std::printf("    'man' ZWJ 'a': the stdlib charges %ld columns, the"
                    " ONE rule says %zu\n", charged2, count_columns(manzwja));
        c.expect_eq(count_columns(manzwja),
                    static_cast<std::size_t>(charged2),
                    "the ONE rule agrees with the linked stdlib here too");
        c.expect_eq(count_columns(manzwja), 3u,
                    "three columns: the man, the ZWJ's cluster, and 'a'");
    }

    // ---- 16.7 the GB11 model against the linked stdlib ----------------
    //
    // The model must agree with the LINKED stdlib on every cell of the
    // battery -- under libc++ 21 AND under libstdc++ 15, with no source
    // change between them.
    {
        std::size_t agree = 0;
        std::vector<std::string> bad;
        for (const Arg& a : args) {
            const long charged = charged_width(a.bytes);
            const std::size_t model = count_columns(a.bytes);
            if (static_cast<long>(model) == charged) {
                ++agree;
            } else if (bad.size() < 6) {
                bad.push_back("    " + std::string{a.label} + ": the model says " +
                              std::to_string(model) + ", the stdlib says " +
                              std::to_string(charged));
            }
        }
        std::printf("\n  the linked stdlib (\"%s\"): the GB11 model agrees on"
                    " %zu of %zu cells\n", linked.name, agree, args.size());
        for (const std::string& s : bad) std::printf("%s\n", s.c_str());
        c.expect_eq(agree, args.size(),
                    "the GB11 model agrees with the linked stdlib on the"
                    " battery");

        // Nov 18 measured the DELETED rule on this same battery, where
        // it lost two cells.  The rule is gone, so there is nothing left
        // to compare: the ONE rule is the arc's only column rule.
        std::printf("    the deleted rule is gone -- the ONE rule stands"
                    " alone\n");
    }
}

// ============================================================================
// Section 17 — THE BOUNDARY BATTERY.
//
// Nov 18's chain, item (1), asked for this:
//
//   "The GB11 fix belongs in the arc's OWN width rule, not beside it.
//    Today the GB11 model lives in `count_columns_gb11()` while the
//    arc's Nov 17 rule still lives in `count_field_width()` -- and the
//    old rule is now known to be wrong on two cells of today's battery.
//    The two should be ONE function, with the old rule deleted, and
//    the whole 28-cell width battery re-measured afterwards.  Worth
//    doing."
//
// CLOSED.  There is ONE column rule now: `count_columns()`.  The old
// rule is DELETED, and the 28-cell width battery (Section 15) and the
// 56-cell precision battery (Section 16) are re-measured with it --
// their numbers (1, 11, 28, and 56 of 56) do not move.
//
// AND THE MERGE PAID FOR ITSELF.  With both rules in ONE place, the
// battery can be widened to the CLUSTER BOUNDARIES the model does not
// implement -- and the model is found to be missing THREE of UAX #29's
// rules: GB9b (Prepend x), GB12/GB13 (the regional-indicator pairs) and
// GB9c (the Indic conjunct).  Three tables and three rules later, the
// ONE rule agrees with the linked stdlib on every cell of the boundary
// battery.
//
//   17.1  the boundary battery: three models, one oracle.
//   17.2  what each of the three new rules is worth.
//   17.3  the ONE rule's tables are RESTRICTED.
// ============================================================================
void section17(counters& c) {
    section("Section 17 — THE BOUNDARY BATTERY");

    const ImplPolicy& linked = linked_policy();

    // ---- 17.1 the battery ---------------------------------------------
    //
    // One row per kind of argument the CLUSTER rules have to cope with:
    // the plain ones the arc already knew, and the ones that exercise a
    // UAX #29 rule the arc's model does not implement.
    struct Cell {
        const char* label;
        std::string bytes;
    };
    const std::vector<Cell> cells = {
        {"ascii 'abc'",                 "abc"},
        {"U+1F600 grinning (W)",        "\xF0\x9F\x98\x80"},
        {"RI pair (GB12)",              "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"},
        {"RI four (GB12)",
         "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7"},
        {"'a' + RI",                    "a\xF0\x9F\x87\xBA"},
        {"U+0600 prepend + 'a' (GB9b)", "\xD8\x80" "a"},
        {"ka virama ssa (GB9c)",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama",                   "\xE0\xA4\x95\xE0\xA5\x8D"},
        {"ka virama + 'a'",             "\xE0\xA4\x95\xE0\xA5\x8D" "a"},
        {"'a' ZWJ U+1F600 (GB11)",      "a\xE2\x80\x8D\xF0\x9F\x98\x80"},
        {"'man' ZWJ 'a' (GB11)",        "\xF0\x9F\x91\xA8\xE2\x80\x8D" "a"},
        {"EP ZWJ EP",                   "\xF0\x9F\x98\x80\xE2\x80\x8D\xF0\x9F\x98\x80"},
        {"family ZWJ (7 cp)",
         "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
         "\xF0\x9F\x91\xA6\xE2\x80\x8D\xF0\x9F\x91\xA7"},
        {"'e' + U+0301 (GB9)",          "e\xCC\x81"},
        {"thumb + skin tone (GB9)",     "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD"},
        {"keycap 1 FE0F 20E3 (GB9)",    "1\xEF\xB8\x8F\xE2\x83\xA3"},
        {"hangul syllable U+AC00",      "\xEA\xB0\x80"},
        {"hangul jamo L V T (GB6/7/8)",
         "\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xAB"},
        {"ka + U+0903 (GB9a)",           "\xE0\xA4\x95\xE0\xA4\x83"},
        {"ka U+0903 + virama ssa",
         "\xE0\xA4\x95\xE0\xA4\x83\xE0\xA5\x8D\xE0\xA4\xB7"},
    };

    // The three models, scored against the width the LINKED stdlib
    // charges:
    //
    //   A  Nov 17's DELETED rule: the code point after a ZWJ joins
    //      unconditionally, and nothing else is implemented.
    //   B  Nov 18's model: GB9 + GB11.
    //   C  today's ONE rule: GB6/7/8 + GB9 + GB9a + GB9b + GB9c + GB11
    //      + GB12/GB13.
    const unsigned model_a = 0u;
    const unsigned model_b = gb::gb11;
    const unsigned model_c = gb::all;

    std::size_t a_agree = 0;
    std::size_t b_agree = 0;
    std::size_t c_agree = 0;

    std::printf("  the boundary battery -- three models, one oracle (%s):\n",
                linked.name);
    std::printf("    %-28s %6s %7s %7s %7s\n", "argument", "stdlib",
                "A(old)", "B(GB11)", "C(ONE)");

    for (const Cell& cl : cells) {
        const long charged = charged_width(cl.bytes);
        const std::size_t a = count_columns(cl.bytes, model_a);
        const std::size_t b = count_columns(cl.bytes, model_b);
        const std::size_t cc = count_columns(cl.bytes, model_c);
        if (static_cast<long>(a) == charged) ++a_agree;
        if (static_cast<long>(b) == charged) ++b_agree;
        if (static_cast<long>(cc) == charged) ++c_agree;
        std::printf("    %-28s %6ld %7zu %7zu %7zu\n", cl.label, charged, a, b,
                    cc);
    }

    std::printf("\n    the models, over %zu cells:\n", cells.size());
    std::printf("      A  Nov 17's DELETED rule agree on %2zu of %zu cells\n",
                a_agree, cells.size());
    std::printf("      B  Nov 18's GB11 model    agree on %2zu of %zu cells\n",
                b_agree, cells.size());
    std::printf("      C  today's ONE rule      agree on %2zu of %zu cells\n",
                c_agree, cells.size());

    c.expect_eq(c_agree, cells.size(),
                "the ONE rule agrees with the linked stdlib on every cell");
    c.expect_eq(b_agree < cells.size(), true,
                "Nov 18's model loses at least one cell of the boundary"
                " battery");
    c.expect_eq(a_agree < b_agree, true,
                "the deleted rule loses more cells than Nov 18's model");

    // ---- 17.2 what each of the three new rules is worth --------------
    //
    // For each rule the ONE rule gained today, turn it OFF and count
    // the cells the model loses.  A rule that were worth nothing would
    // cost nothing to drop.
    struct Rule {
        const char* name;
        unsigned bit;
    };
    const std::vector<Rule> rules = {
        {"GB9a  x SpacingMark", gb::gb9a},
        {"GB9b  Prepend x", gb::gb9b},
        {"GB9c  the Indic conjunct", gb::gb9c},
        {"GB12  the regional-indicator pairs", gb::gb12},
        {"GB68  the Hangul syllable sequences", gb::gb68},
    };
    std::printf("\n  what each rule is worth (the rule turned OFF):\n");
    for (const Rule& r : rules) {
        std::size_t agree_without = 0;
        for (const Cell& cl : cells) {
            if (static_cast<long>(
                    count_columns(cl.bytes, gb::all & ~r.bit)) ==
                charged_width(cl.bytes)) {
                ++agree_without;
            }
        }
        std::printf("    %-38s without it: %2zu of %zu cells\n", r.name,
                    agree_without, cells.size());
        c.expect_eq(agree_without < cells.size(), true,
                    "each rule is worth at least one cell");
    }

    // ---- 17.3 the ONE rule's tables are RESTRICTED --------------------
    //
    // Every table the ONE rule carries is restricted to what the
    // batteries need -- the arc's standing practice, and the reason the
    // restriction is VISIBLE rather than assumed.  libc++ 21 ships the
    // full ones: `__format/width_estimation_table.h` has 275 lines and
    // `__format/extended_grapheme_cluster_table.h` has 1,669.
    std::printf("\n  the ONE rule's tables (RESTRICTED, except the"
                " control table):\n");
    std::printf("    kWidth2Ranges           %3zu ranges\n",
                std::size(kWidth2Ranges));
    std::printf("    kExtendedPictographic   %3zu ranges\n",
                std::size(kExtendedPictographic));
    std::printf("    kPrepend                %3zu ranges\n",
                std::size(kPrepend));
    std::printf("    kIndicLinker            %3zu ranges\n",
                std::size(kIndicLinker));
    std::printf("    kIndicConsonant         %3zu ranges\n",
                std::size(kIndicConsonant));
    std::printf("    kControlRanges          %3zu ranges  (COMPLETE)\n",
                std::size(kControlRanges));
    c.expect_eq(std::size(kWidth2Ranges) > 0u, true,
                "the width table is not empty");
    c.expect_eq(std::size(kPrepend) > 0u, true,
                "the prepend table is not empty");
}

// ---------------------------------------------------------------------------
// Section 18 — the COLLISION BATTERY (NEW TODAY)
// ---------------------------------------------------------------------------
//
// UAX #29's rules are an ORDERED list, and a rule CONSUMES its
// context.  Two things follow, and Nov 19's ONE rule got both wrong:
//
//   1. GB9c is listed BEFORE GB11.  When a conjunct's left context
//      ("Consonant [Extend Linker]* Linker [Extend Linker]*") is live
//      and a ZWJ follows, GB9c decides the boundary -- not GB11.
//      Nov 19's rule asked GB11 first, so "ka" virama ZWJ "ssa" was
//      cut in two.
//
//   2. GB9c CONSUMES its context: the Linker is spent once the
//      conjunct has formed, so a following Consonant needs a Linker of
//      its own.  Nov 19's rule kept the Linker, so "ka" virama "ssa"
//      "ssa" was ONE cluster.
//
// The battery: one row per place where TWO rules can meet.  Four
// models, one oracle: the width the LINKED stdlib charges.
//
//   A  Nov 19's rule: GB11 asked first, the Linker kept.
//   B  A + the conjunct CONSUMES its Linker.
//   C  the ORDER fixed (GB9c asked first), the Linker kept.
//   D  today's ONE rule: the order AND the consumption.
void section18(counters& c) {
    section("Section 18 — the COLLISION BATTERY");

    const ImplPolicy& linked = linked_policy();

    // ---- 18.1 the battery ------------------------------------------------
    //
    // Every cell starts with a NON-ASCII code point, so libc++'s ASCII
    // fast path (which charges one column per ASCII code unit, without
    // any cluster analysis) never applies: the oracle is always the
    // grapheme-cluster algorithm.
    struct Cell {
        const char* label;
        std::string bytes;
    };
    const std::vector<Cell> cells = {
        {"ka virama ssa",            "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama ssa ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA4\xB7"},
        {"ka virama ZWJ ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE2\x80\x8D\xE0\xA4\xB7"},
        {"ka virama ZWJ ZWJ ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE2\x80\x8D\xE2\x80\x8D\xE0\xA4\xB7"},
        {"ka ZWJ ssa",               "\xE0\xA4\x95\xE2\x80\x8D\xE0\xA4\xB7"},
        {"ka ZWJ virama ssa",
         "\xE0\xA4\x95\xE2\x80\x8D\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama ssa ZWJ ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE2\x80\x8D\xE0\xA4\xB7"},
        {"ka virama ZWJ ssa EP",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE2\x80\x8D\xE0\xA4\xB7\xF0\x9F\x98\x80"},
        {"ka virama U+0301 ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xCC\x81\xE0\xA4\xB7"},
        {"ka U+0903 virama ssa",
         "\xE0\xA4\x95\xE0\xA4\x83\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama U+0903 ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\x83\xE0\xA4\xB7"},
        {"ka virama ZWJ U+0903 ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE2\x80\x8D\xE0\xA4\x83\xE0\xA4\xB7"},
        {"ssa virama ssa",           "\xE0\xA4\xB7\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama ssa virama ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama ssa U+0903",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA4\x83"},
        {"prepend ka virama ssa",
         "\xD8\x80\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama RI ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xF0\x9F\x87\xBA\xE0\xA4\xB7"},
        {"ka virama RI RI",
         "\xE0\xA4\x95\xE0\xA5\x8D\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"},
        {"ka virama space ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\x20\xE0\xA4\xB7"},
        {"ka virama EP ssa",
         "\xE0\xA4\x95\xE0\xA5\x8D\xF0\x9F\x98\x80\xE0\xA4\xB7"},
        {"EP ZWJ ka virama ssa",
         "\xF0\x9F\x98\x80\xE2\x80\x8D\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"ka virama ZWJ EP",
         "\xE0\xA4\x95\xE0\xA5\x8D\xE2\x80\x8D\xF0\x9F\x98\x80"},
        {"virama ssa",               "\xE0\xA5\x8D\xE0\xA4\xB7"},
        {"EP ZWJ EP",                "\xF0\x9F\x98\x80\xE2\x80\x8D\xF0\x9F\x98\x80"},
    };

    const unsigned model_a = gb::all & ~gb::gb9c_first & ~gb::gb9c_consume;
    const unsigned model_b = gb::all & ~gb::gb9c_first;
    const unsigned model_c = gb::all & ~gb::gb9c_consume;
    const unsigned model_d = gb::all;

    std::size_t a_agree = 0;
    std::size_t b_agree = 0;
    std::size_t c_agree = 0;
    std::size_t d_agree = 0;

    std::printf("  the collision battery -- four models, one oracle (%s):\n",
                linked.name);
    std::printf("    %-24s %6s %7s %7s %7s %7s\n", "argument", "stdlib",
                "A(old)", "B(cons)", "C(ord)", "D(ONE)");

    for (const Cell& cl : cells) {
        const long charged = charged_width(cl.bytes);
        const std::size_t a = count_columns(cl.bytes, model_a);
        const std::size_t b = count_columns(cl.bytes, model_b);
        const std::size_t cc = count_columns(cl.bytes, model_c);
        const std::size_t d = count_columns(cl.bytes, model_d);
        if (static_cast<long>(a) == charged) ++a_agree;
        if (static_cast<long>(b) == charged) ++b_agree;
        if (static_cast<long>(cc) == charged) ++c_agree;
        if (static_cast<long>(d) == charged) ++d_agree;
        std::printf("    %-24s %6ld %7zu %7zu %7zu %7zu%s\n", cl.label,
                    charged, a, b, cc, d,
                    (static_cast<long>(d) == charged) ? "" : "  <- DIFFERS");
    }

    std::printf("\n    the models, over %zu cells:\n", cells.size());
    std::printf("      A  Nov 19's rule        agree on %2zu of %zu cells\n",
                a_agree, cells.size());
    std::printf("      B  A + the Linker spent agree on %2zu of %zu cells\n",
                b_agree, cells.size());
    std::printf("      C  the ORDER fixed      agree on %2zu of %zu cells\n",
                c_agree, cells.size());
    std::printf("      D  today's ONE rule     agree on %2zu of %zu cells\n",
                d_agree, cells.size());

    c.expect_eq(d_agree, cells.size(),
                "today's ONE rule agrees with the linked stdlib on every "
                "cell of the collision battery");
    c.expect_eq(a_agree < cells.size(), true,
                "Nov 19's rule loses at least one cell of the collision "
                "battery");
    c.expect_eq(b_agree < d_agree, true,
                "spending the conjunct's Linker is worth at least one cell");
    c.expect_eq(c_agree < d_agree, true,
                "asking GB9c before GB11 is worth at least one cell");
    c.expect_eq(a_agree < b_agree, true,
                "Nov 19's rule loses more cells than the model that only "
                "spends the Linker");
    c.expect_eq(a_agree < c_agree, true,
                "Nov 19's rule loses more cells than the model that only "
                "fixes the order");

    // ---- 18.2 what each of today's two changes is worth ------------------
    //
    // Each fix on its own: turn it OFF and count the cells the model
    // loses.  A change that were worth nothing would cost nothing to
    // drop.
    struct Change {
        const char* name;
        unsigned bit;
    };
    const std::vector<Change> changes = {
        {"the ORDER  (GB9c before GB11)", gb::gb9c_first},
        {"the CONSUMPTION (the Linker is spent)", gb::gb9c_consume},
    };
    std::printf("\n  what each of today's two changes is worth:\n");
    for (const Change& ch : changes) {
        std::size_t agree_without = 0;
        for (const Cell& cl : cells) {
            if (static_cast<long>(
                    count_columns(cl.bytes, gb::all & ~ch.bit)) ==
                charged_width(cl.bytes)) {
                ++agree_without;
            }
        }
        std::printf("    %-42s without it: %2zu of %zu cells\n", ch.name,
                    agree_without, cells.size());
        c.expect_eq(agree_without < cells.size(), true,
                    "each of today's two changes is worth at least one cell");
    }

    // ---- 18.3 the control cells -- CLOSED by Nov 21 -------------------
    //
    // These four cells were Nov 20's open gap: the control characters
    // are UAX #29's GB3/GB4/GB5, and the arc did not model them at
    // all.  Nov 21 models them -- in UAX #29's order -- and every one
    // of the four now agrees with the linked stdlib.  They are still
    // printed, so the CLOSURE is visible rather than assumed.
    const std::vector<Cell> control_cells = {
        {"ka CRLF ka",
         std::string("\xE0\xA4\x95\x0D\x0A\xE0\xA4\x95", 8)},
        {"ka CRLF",   std::string("\xE0\xA4\x95\x0D\x0A", 5)},
        {"ka CRLF CRLF ka",
         std::string("\xE0\xA4\x95\x0D\x0A\x0D\x0A\xE0\xA4\x95", 10)},
        {"ka NUL U+0301 ka",
         std::string("\xE0\xA4\x95\x00\xCC\x81\xE0\xA4\x95", 9)},
    };
    std::size_t control_lost = 0;
    std::printf("\n  Nov 20's control cells -- CLOSED by Nov 21:\n");
    for (const Cell& cl : control_cells) {
        const long charged = charged_width(cl.bytes);
        const std::size_t d = count_columns(cl.bytes, gb::all);
        if (static_cast<long>(d) != charged) ++control_lost;
        std::printf("    %-24s %6ld %7zu%s\n", cl.label, charged, d,
                    (static_cast<long>(d) == charged) ? "" : "  <- DIFFERS");
    }
    c.expect_eq(control_lost, 0u,
                "the control cells are closed: GB3/GB4/GB5 are modelled "
                "and the ONE rule agrees with the linked stdlib");
    std::printf("\n    %zu of %zu control cells still differ\n", control_lost,
                control_cells.size());
}

// ============================================================================
// Section 19 — THE CONTROL BATTERY (NEW TODAY)
// ============================================================================
//
// UAX #29's GB3, GB4 and GB5 are about the CONTROL characters, and Nov
// 20's rule did not model them at all:
//
//   GB3   CR x LF                do not break between a CR and an LF
//   GB4   (Control | CR | LF) v     break AFTER a control
//   GB5   v (Control | CR | LF)     break BEFORE a control
//
// Two things came out of modelling them.
//
//   1. THE ORDER.  GB3, GB4 and GB5 come BEFORE GB9 in UAX #29's
//      ordered list, so an Extend after a NUL does NOT join the NUL's
//      cluster -- the control breaks on BOTH sides.  This is Nov 20's
//      lesson again, one rule list further down.
//
//   2. THE ASCII FAST PATH.  libc++ 21 does not cluster an ASCII
//      prefix at all: it charges one column per CODE UNIT.  The only
//      cluster the ASCII rules can form is GB3's CR x LF, so the fast
//      path is visible exactly there -- a CR LF pair is TWO columns at
//      the start of an ASCII string and ONE column in the middle of a
//      non-ASCII-leading one.
//
// The battery: cells where a control meets another rule, and cells
// where the ASCII fast path and the cluster rules disagree.  Four
// models, one oracle: the width the LINKED stdlib charges.
//
//   A  Nov 20's rule: no control rules, no fast path.
//   B  the control rules, but consulted AFTER GB9 (the wrong order).
//   C  the control rules in UAX #29's order, and no fast path.
//   D  today's ONE rule: the order AND the fast path.
//
//   19.1  the battery: four models, one oracle.
//   19.2  the ASCII FAST PATH, and the PRECISION it cuts.
//   19.3  what each of today's changes is worth.
//   19.4  the control table, and the tables that are still RESTRICTED.
// ============================================================================
void section19(counters& c) {
    section("Section 19 — THE CONTROL BATTERY");

    const ImplPolicy& linked = linked_policy();

    struct Cell {
        const char* label;
        std::string bytes;
    };
    // A cell's bytes are given one BYTE at a time, because a control
    // character is a NUL in the middle of a string, and a C string
    // literal would stop there.
    const auto bytes = [](std::initializer_list<int> bs) {
        std::string s;
        for (int b : bs) s.push_back(static_cast<char>(b));
        return s;
    };
    const std::vector<Cell> cells = {
        // -- the control rules where the string does NOT start with
        //    ASCII, so the fast path cannot apply and the oracle is the
        //    cluster algorithm itself.  "ka" is the Devanagari KA
        //    (U+0915): three bytes, one column.
        {"ka CRLF ka",         bytes({0xE0,0xA4,0x95,0x0D,0x0A,0xE0,0xA4,0x95})},
        {"ka CRLF",            bytes({0xE0,0xA4,0x95,0x0D,0x0A})},
        {"ka CRLF CRLF ka",    bytes({0xE0,0xA4,0x95,0x0D,0x0A,0x0D,0x0A,0xE0,0xA4,0x95})},
        {"ka NUL U+0301 ka",   bytes({0xE0,0xA4,0x95,0x00,0xCC,0x81,0xE0,0xA4,0x95})},
        {"ka CR U+0301 ka",    bytes({0xE0,0xA4,0x95,0x0D,0xCC,0x81,0xE0,0xA4,0x95})},
        {"ka U+0301 NUL ka",   bytes({0xE0,0xA4,0x95,0xCC,0x81,0x00,0xE0,0xA4,0x95})},
        {"ka NUL NUL ka",      bytes({0xE0,0xA4,0x95,0x00,0x00,0xE0,0xA4,0x95})},
        {"ka TAB ka",          bytes({0xE0,0xA4,0x95,0x09,0xE0,0xA4,0x95})},
        {"ka ESC ka",          bytes({0xE0,0xA4,0x95,0x1B,0xE0,0xA4,0x95})},
        {"ka SHY ka",          bytes({0xE0,0xA4,0x95,0xC2,0xAD,0xE0,0xA4,0x95})},
        {"ka LS ka",           bytes({0xE0,0xA4,0x95,0xE2,0x80,0xA8,0xE0,0xA4,0x95})},
        {"ka NEL ka",          bytes({0xE0,0xA4,0x95,0xC2,0x85,0xE0,0xA4,0x95})},
        {"ka U+0600 NUL ka",   bytes({0xE0,0xA4,0x95,0xD8,0x80,0x00,0xE0,0xA4,0x95})},
        // -- the same rules where the string DOES start with ASCII, so
        //    libc++'s fast path applies and the CR LF pair is charged
        //    per code unit.
        {"a CRLF a",           bytes({0x61,0x0D,0x0A,0x61})},
        {"CR LF",              bytes({0x0D,0x0A})},
        {"a CRLF CRLF a",      bytes({0x61,0x0D,0x0A,0x0D,0x0A,0x61})},
        {"a NUL a",            bytes({0x61,0x00,0x61})},
        {"a CRLF ka",          bytes({0x61,0x0D,0x0A,0xE0,0xA4,0x95})},
        {"a CRLF U+0301 a",    bytes({0x61,0x0D,0x0A,0xCC,0x81,0x61})},
    };

    const unsigned model_a = gb::all & ~gb::gb3 & ~gb::gb4 & ~gb::gb5 &
                             ~gb::ascii_fast;
    const unsigned model_b = gb::all & ~gb::gb_control_first & ~gb::ascii_fast;
    const unsigned model_c = gb::all & ~gb::ascii_fast;
    const unsigned model_d = gb::all;

    std::size_t a_agree = 0;
    std::size_t b_agree = 0;
    std::size_t c_agree = 0;
    std::size_t d_agree = 0;
    std::size_t a_only = 0;
    std::size_t b_only = 0;

    std::printf("  the control battery -- four models, one oracle (%s):\n",
                linked.name);
    std::printf("    %-22s %6s %7s %7s %7s %7s\n", "argument", "stdlib",
                "A(old)", "B(late)", "C(ord)", "D(ONE)");

    for (const Cell& cl : cells) {
        const long charged = charged_width(cl.bytes);
        const std::size_t a = count_columns(cl.bytes, model_a);
        const std::size_t b = count_columns(cl.bytes, model_b);
        const std::size_t cc = count_columns(cl.bytes, model_c);
        const std::size_t d = count_columns(cl.bytes, model_d);
        const bool a_ok = static_cast<long>(a) == charged;
        const bool b_ok = static_cast<long>(b) == charged;
        if (a_ok) ++a_agree;
        if (b_ok) ++b_agree;
        if (static_cast<long>(cc) == charged) ++c_agree;
        if (static_cast<long>(d) == charged) ++d_agree;
        if (a_ok && !b_ok) ++a_only;
        if (b_ok && !a_ok) ++b_only;
        std::printf("    %-22s %6ld %7zu %7zu %7zu %7zu%s\n", cl.label,
                    charged, a, b, cc, d,
                    (static_cast<long>(d) == charged) ? "" : "  <- DIFFERS");
    }

    std::printf("\n    the models, over %zu cells:\n", cells.size());
    std::printf("      A  Nov 20's rule         agree on %2zu of %zu cells\n",
                a_agree, cells.size());
    std::printf("      B  the WRONG order       agree on %2zu of %zu cells\n",
                b_agree, cells.size());
    std::printf("      C  UAX #29's order       agree on %2zu of %zu cells\n",
                c_agree, cells.size());
    std::printf("      D  today's ONE rule      agree on %2zu of %zu cells\n",
                d_agree, cells.size());
    std::printf("      A wins %zu cells B loses; B wins %zu cells A loses\n",
                a_only, b_only);

    c.expect_eq(d_agree, cells.size(),
                "today's ONE rule agrees with the linked stdlib on every "
                "cell of the control battery");
    c.expect_eq(a_agree < d_agree, true,
                "Nov 20's rule loses cells of the control battery");
    c.expect_eq(b_agree < d_agree, true,
                "the wrong order loses cells of the control battery");
    c.expect_eq(c_agree < d_agree, true,
                "the ASCII fast path is worth cells of the control battery");
    c.expect_eq(c_agree > b_agree, true,
                "UAX #29's order is worth cells over the wrong order");
    c.expect_eq(a_only > 0u, true,
                "the control rules win cells that Nov 20's rule loses");
    c.expect_eq(b_only > 0u, true,
                "the fast path wins cells that Nov 20's rule loses");

    // ---- 19.2 the ASCII FAST PATH --------------------------------------
    //
    // The fast path is the LINKED STDLIB's shortcut, and it is visible
    // exactly where the ASCII rules can form a cluster: GB3's CR x LF.
    // The "pure" column is the cluster algorithm alone; the ONE rule is
    // libc++'s algorithm.
    const std::vector<Cell> fast_cells = {
        {"CR LF",         bytes({0x0D,0x0A})},
        {"a CRLF a",      bytes({0x61,0x0D,0x0A,0x61})},
        {"a CRLF CRLF a", bytes({0x61,0x0D,0x0A,0x0D,0x0A,0x61})},
        {"ka CRLF ka",    bytes({0xE0,0xA4,0x95,0x0D,0x0A,0xE0,0xA4,0x95})},
    };
    std::printf("\n  the ASCII FAST PATH -- the cluster rules against the"
                " shortcut:\n");
    std::printf("    %-16s %6s %8s %8s\n", "argument", "stdlib", "pure",
                "ONE");
    for (const Cell& cl : fast_cells) {
        const long charged = charged_width(cl.bytes);
        const std::size_t pure =
            count_columns(cl.bytes, gb::all & ~gb::ascii_fast);
        const std::size_t one = count_columns(cl.bytes, gb::all);
        std::printf("    %-16s %6ld %8zu %8zu%s\n", cl.label, charged, pure,
                    one, (static_cast<long>(one) == charged) ? "" : "  <- DIFFERS");
        c.expect_eq(static_cast<std::size_t>(one),
                    static_cast<std::size_t>(charged),
                    "the ONE rule agrees with the linked stdlib on the fast "
                    "path cells");
    }

    // The same CR LF pair, in two places.
    const long crlf_ascii = charged_width(bytes({0x0D, 0x0A}));
    const long crlf_inside =
        charged_width(bytes({0xE0,0xA4,0x95,0x0D,0x0A,0xE0,0xA4,0x95}));
    std::printf("\n    the SAME CR LF pair, in two places:\n");
    std::printf("      \"CR LF\"      (ASCII first)     %ld columns\n",
                crlf_ascii);
    std::printf("      \"ka CRLF ka\" (non-ASCII first) %ld columns\n",
                crlf_inside);
    c.expect_eq(static_cast<std::size_t>(crlf_ascii), 2u,
                "at the start of an ASCII string the fast path charges the "
                "CR LF pair two columns");
    c.expect_eq(static_cast<std::size_t>(crlf_inside), 3u,
                "inside a non-ASCII-leading string the CR LF pair is one "
                "cluster, so it is one column");

    // And the PRECISION, where the fast path cuts at a CODE UNIT.
    {
        const std::string arg = bytes({0x61, 0x0D, 0x0A, 0x61});
        const std::string out =
            std::vformat("{:.2}", std::make_format_args(arg));
        std::printf("\n    the PRECISION, and the fast path's cut:\n");
        std::printf("      \"a\" CRLF \"a\" with precision 2 -> %zu byte(s)",
                    out.size());
        for (char ch : out) {
            const unsigned u = static_cast<unsigned char>(ch);
            if (u < 0x20u || u == 0x7Fu)
                std::printf(" \\x%02X", u);
            else
                std::printf(" %c", ch);
        }
        std::printf("\n");
        c.expect_eq(out.size(), 2u,
                    "the stdlib's precision cuts the CR LF pair in half");
        c.expect_eq(prefix_bytes_for_precision(arg, 2u), 2u,
                    "the ONE rule's precision agrees with the linked stdlib");
    }

    // ---- 19.3 what each of today's changes is worth --------------------
    struct Change {
        const char* name;
        unsigned bit;
    };
    const std::vector<Change> changes = {
        {"GB3  (CR x LF)",                 gb::gb3},
        {"GB4  (break AFTER a control)",    gb::gb4},
        {"GB5  (break BEFORE a control)",   gb::gb5},
        {"the ORDER (GB4/GB5 before GB9)",  gb::gb_control_first},
        {"the ASCII FAST PATH",             gb::ascii_fast},
    };
    std::printf("\n  what each of today's changes is worth:\n");
    for (const Change& ch : changes) {
        std::size_t agree_without = 0;
        for (const Cell& cl : cells) {
            if (static_cast<long>(
                    count_columns(cl.bytes, gb::all & ~ch.bit)) ==
                charged_width(cl.bytes)) {
                ++agree_without;
            }
        }
        std::printf("    %-42s without it: %2zu of %zu cells\n", ch.name,
                    agree_without, cells.size());
        c.expect_eq(agree_without < cells.size(), true,
                    "each of today's changes is worth at least one cell");
    }

    // ---- 19.4 the control table, and the tables that are RESTRICTED ----
    std::printf("\n  the control table -- COMPLETE, unlike the others:\n");
    std::printf("    kControlRanges          %3zu ranges  (Control, CR, LF)\n",
                std::size(kControlRanges));
    c.expect_eq(std::size(kControlRanges), 21u,
                "the control table is the complete set of Control, CR and LF "
                "ranges");
    std::printf("    the width, emoji, prepend and Indic tables are still"
                " RESTRICTED\n");
}


}  // namespace

int main() {
    counters c;

    std::printf("P-2026-11-21 — try_format_control_chars\n");
    std::printf("GB3/GB4/GB5 come BEFORE GB9, and libc++'s ASCII fast\n"
                "path charges a code unit, not a cluster.\n");
#if defined(_LIBCPP_VERSION)
    std::printf("built against libc++ %d\n", _LIBCPP_VERSION);
#elif defined(_GLIBCXX_RELEASE)
    std::printf("built against libstdc++ %d\n", _GLIBCXX_RELEASE);
#else
    std::printf("built against an unknown standard library\n");
#endif

    section1(c);
    section2(c);
    section3(c);
    section4(c);
    section5(c);
    section6(c);
    section7(c);
    section8(c);
    section9(c);
    section10(c);
    section11(c);
    section12(c);
    section13(c);
    section14(c);
    section15(c);
    section16(c);
    section17(c);
    section18(c);
    section19(c);

    std::printf("\n=== summary ===\n");
    std::printf("  pass=%zu  fail=%zu\n", c.pass, c.fail);
    return c.fail == 0 ? 0 : 1;
}
