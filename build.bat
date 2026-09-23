@echo off
rem Standalone build of the it_from_bit variant: pdflatex -> biber -> pdflatex x2.
rem Expects: it_from_bit.tex, manuscript.bib, fig1.png (all in this directory).
cd /d E:\it_from_bit
pdflatex -interaction=nonstopmode it_from_bit.tex
biber it_from_bit
pdflatex -interaction=nonstopmode it_from_bit.tex
pdflatex -interaction=nonstopmode it_from_bit.tex
it_from_bit.pdf
