#!/bin/bash
cp -r sprites/actor sprites/clone
gimp -i -b '(colorize "sprites/clone/*.png" 191 70 0)' -b '(gimp-quit 0)'
cp -r sprites/actor sprites/clone2
gimp -i -b '(colorize "sprites/clone2/*.png" 290 70 0)' -b '(gimp-quit 0)'
