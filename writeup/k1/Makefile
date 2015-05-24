all: k1.pdf

k1.pdf: k1.tex README.tex
	pdflatex -shell-escape k1.tex

README.tex:
	cat ../../README.md | sed -e '1,/BEGIN LaTeX INCLUDED/d' | pandoc -f markdown -t latex > README.tex
