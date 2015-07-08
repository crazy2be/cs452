all: README.tex
	pdflatex -shell-escape tc1.tex

README.tex:
	cat ../../README.md | sed -e '1,/BEGIN LaTeX INCLUDED/d' | pandoc -f markdown -t latex > README.tex
