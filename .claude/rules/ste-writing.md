# Writing rule: Simplified Technical English (ASD-STE100)

Write all documents in this repository in Simplified Technical English (STE).
This includes CLAUDE.md, the rules, docs/, the tutorials, commit messages
and comments in code. Tables, file names, code identifiers, numbers and
proper names are not sentences; the rules do not apply to them.

## The principles
- Use an approved word only in its approved meaning. One word, one meaning.
- Use technical names and technical verbs from the domain when there is no
  approved word (hex, arena, officer, decompress, commit).
- Write short sentences. Procedures: 20 words or fewer. Descriptions: 25
  words or fewer. One instruction in each sentence.
- Write in the active voice. Say who does what: "The loader reads the
  header", not "The header is read".
- Use the present tense for descriptions and the imperative for
  instructions: "Open the file", not "The file should be opened".
- Give one topic to each paragraph. Keep paragraphs to six sentences.
- Put a condition before the instruction: "If the file is empty, stop."
- Put warnings and cautions before the step they apply to.
- Use vertical lists for sequences and for sets of more than three items.
- Use articles ("the", "a") and demonstratives ("this", "these"). Do not
  drop them to save space.
- Do not use more than three nouns in a row. Break the cluster with a
  preposition: "the offset of the officer table", not "officer table offset".
- Do not use the -ing form of a verb as a verb or a noun. Say "to decode",
  "the decoder", "when you decode".
- Do not use slang, idioms, metaphors or humour.

## Words to replace
| Do not write | Write |
|---|---|
| utilize, employ, leverage | use |
| perform, carry out, execute (a task) | do |
| ensure | make sure |
| in order to | to |
| prior to | before |
| subsequent to, following | after |
| commence, initiate | start |
| terminate | stop |
| assist | help |
| require (a thing) | need |
| adequate, enough | sufficient |
| about, around (a quantity) | approximately |
| should, could, would, may | can (ability), must (obligation), or write the fact |
| shall | must |
| e.g. | for example |
| i.e. | that is |
| etc., and so on | list the items |
| and/or | "A, or B, or both" |
| via | through, by |
| in case of | if |
| once (as "after") | after, when |
| very, quite, rather | delete the word |
| which (without a comma) | that |

## Structure of a document
- Start with one sentence that says what the document is for.
- Put the most important fact first in each section.
- Give each heading a noun phrase, not a question.
- End a procedure with the result the reader can check.

## Self-check before you commit
1. Read each sentence. If it has more than 25 words, split it.
2. Find each verb. If it is passive, make it active.
3. Find each -ing word. Replace it unless it is a technical name.
4. Find "should", "may", "etc.", "e.g." and "via". Replace them.
5. Make sure each paragraph has one topic.
