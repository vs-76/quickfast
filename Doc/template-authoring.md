<!-- Copyright (c) 2026, QuickFAST contributors. -->
<!-- All rights reserved. -->
<!-- See the file license.txt for licensing information. -->

# Template authoring notes

Behaviours a template author needs to know about that are not obvious from the
FAST specification, because they concern how QuickFAST presents a decoded
message rather than how the bytes on the wire are laid out.

## `typeRef` is not an annotation

A group whose application type equals its parent's is **folded** into the
parent when decoding: its fields are added directly to the enclosing field set
and the group does not appear in the decoded message. This is deliberate. A
group is an artifact of the template rather than a property of the message, and
the same message encoded with two different templates may distribute the same
fields across different groups.

The application type is set by `typeRef`, and a group without one does not
inherit its parent's. So `typeRef` decides whether a consumer reads

```cpp
message.getField("b", field);          // folded
message.getField("grp", field);        // not folded
field->toGroup()->getField("b", inner);
```

Adding a `typeRef` to document a group's type will move every field in that
group one level deeper in every decoded message. Adding one to the enclosing
`template` will do the same to every group inside it that has no `typeRef` of
its own, because the comparison is then between the declared type and the
group's default.

Neither side reports anything. The encoding is unaffected; only the shape of
the decoded message changes, so consumer code that walked to a field by name
stops finding it and there is no error to explain why.

| template `typeRef` | group `typeRef` | decoded shape |
|---|---|---|
| absent | absent | `message["b"]` |
| `X` | `X` | `message["b"]` |
| `X` | `Y` | `message["grp"]["b"]` |
| `X` | absent | `message["grp"]["b"]` |
| absent | `Y` | `message["grp"]["b"]` |

The folding is decode-side only. When encoding, a group is always supplied as a
group: build a `Messages::Group`, wrap it with `Messages::FieldGroup::create`,
and add it to the message under the group's name regardless of which row above
applies.

All five rows are pinned by `tests/testGroupTypeRef.cpp`.
