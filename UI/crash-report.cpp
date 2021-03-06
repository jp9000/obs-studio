#include "crash-report.hpp"
#include <QApplication>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include "qt-wrappers.hpp"

OBSCrashReport::OBSCrashReport(QWidget *parent, const char *text)
	: QDialog(parent)
{
	QPushButton *copyButton = new QPushButton;
	copyButton->setText("Copy crash log");

	QPushButton *exitButton = new QPushButton;
	exitButton->setText("Exit");

	textBox = new QPlainTextEdit;
	textBox->setPlainText(QT_UTF8(text));
	textBox->setLineWrapMode(QPlainTextEdit::NoWrap);
	textBox->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget(copyButton);
	buttonLayout->addWidget(exitButton);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(textBox);
	mainLayout->addItem(buttonLayout);

	setLayout(mainLayout);

	QWidget::connect(copyButton, SIGNAL(clicked()), this,
			 SLOT(CopyClicked()));
	QWidget::connect(exitButton, SIGNAL(clicked()), this,
			 SLOT(ExitClicked()));

	resize(800, 600);
	setWindowTitle("Oops, OBS has crashed!");
}

void OBSCrashReport::ExitClicked()
{
	exit(-1);
}

void OBSCrashReport::CopyClicked()
{
	QApplication::clipboard()->setText(textBox->toPlainText());
}
