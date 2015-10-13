#!/bin/bash

FORE=$1
BACK=$2
FILL=$3

if [ "$FORE" = "" ]; then
    FORE=DefaultFore
fi
if [ "$BACK" = "" ]; then
    BACK=DefaultBack
fi

# To detect color changes, we want a character that fills the whole cell
# if possible.  U+2588 is perfect, except that it becomes invisible in the
# original xterm, when bolded.  For that terminal, use something else, like
# "#" or "@".
if [ "$FILL" = "" ]; then
    FILL="█"
fi

# SGR (Select Graphic Rendition)
s() {
    printf '\033[0m'
    while [ "$1" != "" ]; do
        printf '\033['"$1"'m'
        shift
    done
}

# Print
p() {
    echo -n "$@"
}

# Print with newline
pn() {
    echo "$@"
}

# For practical reasons, sandwich black and white in-between the other colors.
FORE_COLORS="31 30 37 32 33 34 35 36"
BACK_COLORS="41 40 47 42 43 44 45 46"



### Test order of Invert(7) -- it does not matter what order it appears in.

# The Red color setting here (31) is shadowed by the green setting (32).  The
# Reverse flag does not cause (32) to alter the background color immediately;
# instead, the Reverse flag is applied once to determine the final effective
# Fore/Back colors.
s 7 31 32; p " -- Should be: $BACK-on-green -- "; s; pn
s 31 7 32; p " -- Should be: $BACK-on-green -- "; s; pn
s 31 32 7; p " -- Should be: $BACK-on-green -- "; s; pn

# As above, but for the background color.
s 7 41 42; p " -- Should be: green-on-$FORE -- "; s; pn
s 41 7 42; p " -- Should be: green-on-$FORE -- "; s; pn
s 41 42 7; p " -- Should be: green-on-$FORE -- "; s; pn

# One last, related test
s 7; p "Invert text"; s 7 1; p " with some words bold"; s; pn;
s 0; p "Normal text"; s 0 1; p " with some words bold"; s; pn;

pn



### Test effect of Bold(1) on color, with and without Invert(7).

# The Bold flag does not affect the background color when Reverse is missing.
# There should always be 8 colored boxes.
p "  "
for x in $BACK_COLORS; do
    s $x; p "-"; s $x 1; p "-"
done
s; pn "    Bold should not affect background"

# On some terminals, Bold affects color, and on some it doesn't.  If there
# are only 8 colored boxes, then the next two tests will also show 8 colored
# boxes.  If there are 16 boxes, then exactly one of the next two tests will
# also have 16 boxes.
p "  "
for x in $FORE_COLORS; do
    s $x; p "$FILL"; s $x 1; p "$FILL"
done
s; pn "    Does bold affect foreground color?"

# On some terminals, Bold+Invert highlights the final Background color.
p "  "
for x in $FORE_COLORS; do
    s $x 7; p "-"; s $x 7 1; p "-"
done
s; pn "    Test if Bold+Invert affects background color"

# On some terminals, Bold+Invert highlights the final Foreground color.
p "  "
for x in $BACK_COLORS; do
    s $x 7; p "$FILL"; s $x 7 1; p "$FILL"
done
s; pn "    Test if Bold+Invert affects foreground color"

pn



### Test for support of ForeHi and BackHi properties.

# ForeHi
p "  "
for x in $FORE_COLORS; do
    hi=$(( $x + 60 ))
    s $x; p "$FILL"; s $hi; p "$FILL"
done
s; pn "    Test for support of ForeHi colors"
p "  "
for x in $FORE_COLORS; do
    hi=$(( $x + 60 ))
    s $x; p "$FILL"; s $x $hi; p "$FILL"
done
s; pn "    Test for support of ForeHi colors (w/compat)"

# BackHi
p "  "
for x in $BACK_COLORS; do
    hi=$(( $x + 60 ))
    s $x; p "-"; s $hi; p "-"
done
s; pn "    Test for support of BackHi colors"
p "  "
for x in $BACK_COLORS; do
    hi=$(( $x + 60 ))
    s $x; p "-"; s $x $hi; p "-"
done
s; pn "    Test for support of BackHi colors (w/compat)"

pn



### Identify the default fore and back colors.

pn "Match default fore and back colors against 16-color palette"
pn "  ==fore==  ==back=="
for fore in $FORE_COLORS; do
    forehi=$(( $fore + 60 ))
    back=$(( $fore + 10 ))
    backhi=$(( $back + 60 ))
    p "  "
    s $fore;   p "$FILL"; s; p "$FILL"; s $fore;   p "$FILL"; s; p "  "
    s $forehi; p "$FILL"; s; p "$FILL"; s $forehi; p "$FILL"; s; p "  "
    s $back;   p "-"; s; p "-"; s $back;   p "-"; s; p "  "
    s $backhi; p "-"; s; p "-"; s $backhi; p "-"; s; p "  "
    pn "  $fore $forehi $back $backhi"
done

pn



### Test coloring of rest-of-line.

#
# When a new line is scrolled in, every cell in the line receives the
# current background color, which can be the default/transparent color.
#

p "Newline with red background: usually no red -->"; s 41; pn
s; pn "This text is plain, but rest is red if scrolled -->"
s; p " "; s 41; printf '\033[1K'; s; printf '\033[1C'; pn "<-- red Erase-in-Line to beginning"
s; p "red Erase-in-Line to end -->"; s 41; printf '\033[0K'; s; pn
pn



### Moving the cursor around does not change colors of anything.

pn "Test modifying uncolored lines with a colored SGR:"
pn "aaaa"
pn
pn "____e"
s 31 42; printf '\033[4C\033[3A'; pn "bb"
pn "cccc"
pn "dddd"
s; pn
