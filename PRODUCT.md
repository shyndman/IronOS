# Product

<!-- impeccable:product-schema 1 -->

## Platform

adaptive

## Users

Scott uses a handheld soldering iron while working directly with electronics. The interface must remain readable and operable while the iron is in hand.

## Product Purpose

IronOS controls a soldering iron and communicates its current state, target temperature, settings, confirmations, and faults on the device display.

## Operating Context

This is embedded firmware, not an iOS or Android application. The current redesign target is a physical 20×10 mm, 160×80-pixel color display operated with left, right, and OK buttons.

## Capabilities and Constraints

- Four simultaneous palette entries through a 2bpp framebuffer.
- Raster typography and graphics only.
- The selected scope is temperature adjustment, restore-defaults confirmation, and settings help.
- Each selected screen focuses exclusively on its purpose.
- Existing fonts and visual language are not constraints.
- `adaptive` is only the nearest category accepted by the design tooling; mobile-platform guidance does not apply.

## Evidence on Hand

- Firmware implementation under `source/Core/`.
- Hardware UI concepts in `../design/design.pen`.
- No claims, customer evidence, or external brand assets are required for the selected screens.

## Product Principles

- Read the essential state at a glance.
- Make each physical control's effect unambiguous.
- Spend every pixel on the current task.
- Prefer deliberate raster forms over compressed app conventions.
