#!/bin/bash
cp -r sprites/actor sprites/clone
gimp -i -b '(colorize "sprites/clone/*.png" 191 70 0)' -b '(gimp-quit 0)'
