# Validation and delivery

Run checks from the Presenter repository or use equivalent installed paths.

## Structural validation

```bash
xmllint --noout --dtdvalid schemas/presentation.dtd "My Talk.slides/presentation.xml"
./build/presenter --check --json --strict "My Talk.slides"
```

Strict checking must finish without warnings or errors before delivery. If the deck uses a custom style, validate it against `schemas/style.dtd` as well.

## Visual review

Render every slide at the intended canvas size:

```bash
./build/presenter "My Talk.slides" --slide 1 --screenshot /tmp/slide-01.png
./build/presenter "My Talk.slides" --slide 1 --presenter-screenshot /tmp/notes-01.png
```

Repeat for every slide. Inspect slides individually and then as a sequence. Check for clipped or wrapped text, unintended overlap, missing images, distorted crops, weak contrast, inconsistent margins, unresolved placeholders, and notes that do not match the audience slide.

## Tests

When changing the skill only, run the skill validator. When changing Presenter behavior or schemas, also run the repository tests:

```bash
make test
```

## Delivery

Deliver the complete `.slides` directory rather than only `presentation.xml`. Keep temporary renders and validation output outside the package unless the user requests them.
