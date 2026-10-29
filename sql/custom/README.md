# Instructions
This is where custom content changes should be placed. These files must be executed last, and must be able to be repeatedly
applied without destroying the database.

New content of this fork goes to `repack/`: every sql change of the fork is a file of that directory, applied by
hand. See `repack/README.md` for the order in which they are applied and for why nothing is added to
`sql/migrations/`.
