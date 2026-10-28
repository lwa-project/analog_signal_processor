# Makefile for LWA ICD Documentation
# Usage:
#   make          - Build all documents
#   make clean    - Remove build artifacts
#   make distclean - Remove all generated files including PDFs

LATEX = pdflatex
LATEX_FLAGS = -interaction=nonstopmode -halt-on-error

# Documents
ASP_ICD_DIR = ASP_ICD
ASP_ICD_DOC = ASP_ICD

.PHONY: all clean distclean asp_icd

# Default target
all: asp_icd

# ASP ICD document
asp_icd: $(ASP_ICD_DOC).pdf

$(ASP_ICD_DOC).pdf: $(ASP_ICD_DIR)/lwa-icd.cls \
                    $(ASP_ICD_DIR)/$(ASP_ICD_DOC).tex \
                    $(wildcard $(ASP_ICD_DIR)/config/*.tex) \
                    $(wildcard $(ASP_ICD_DIR)/frontmatter/*.tex) \
                    $(wildcard $(ASP_ICD_DIR)/sections/*.tex) \
                    $(wildcard $(ASP_ICD_DIR)/appendices/*.tex)
	cd $(ASP_ICD_DIR) && $(LATEX) $(LATEX_FLAGS) $(ASP_ICD_DOC).tex
	cd $(ASP_ICD_DIR) && $(LATEX) $(LATEX_FLAGS) $(ASP_ICD_DOC).tex
	mv $(ASP_ICD_DIR)/$(ASP_ICD_DOC).pdf .

# Clean build artifacts
clean:
	cd $(ASP_ICD_DIR) && rm -f *.aux *.log *.out *.toc *.lof *.lot *.fls *.fdb_latexmk *.synctex.gz
	cd $(ASP_ICD_DIR) && rm -f sections/*.aux appendices/*.aux frontmatter/*.aux config/*.aux

# Remove all generated files including PDFs
distclean: clean
	rm -f $(ASP_ICD_DOC).pdf

# Help
help:
	@echo "LWA ICD Documentation Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build all documents (default)"
	@echo "  asp_icd    - Build ASP ICD document"
	@echo "  clean      - Remove build artifacts"
	@echo "  distclean  - Remove all generated files including PDFs"
	@echo "  help       - Show this help message"
