#include "update-window.hpp"
#include "obs-app.hpp"

OBSUpdate::OBSUpdate(QWidget *parent, bool manualUpdate, const QString &text)
	: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint |
				  Qt::WindowCloseButtonHint),
	  ui(new Ui_OBSUpdate)
{
	ui->setupUi(this);
	ui->text->setHtml(text);

	if (manualUpdate) {
		delete ui->skip;
		ui->skip = nullptr;

		ui->no->setText(QTStr("Cancel"));
	}
}

void OBSUpdate::on_yes_clicked()
{
	done(OBSUpdate::Yes);
}

void OBSUpdate::on_no_clicked()
{
	done(OBSUpdate::No);
}

void OBSUpdate::on_skip_clicked()
{
	done(OBSUpdate::Skip);
}

void OBSUpdate::accept()
{
	done(OBSUpdate::Yes);
}

void OBSUpdate::reject()
{
	done(OBSUpdate::No);
}
