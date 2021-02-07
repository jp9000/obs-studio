#include <QtTest/QtTest>
#include <qformlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qobject.h>
#include "obs-app.hpp"
#include "obs-data.h"
#include "obs-properties.h"
#include "properties-view.hpp"

class TestPropertiesView : public QObject {
	Q_OBJECT

private slots:
	void noPropertiesCreatesNoWidgets()
	{
		OBSPropertiesView view =
			makeView(obs_data_create(), [](obs_properties_t *) {});
		view.dumpObjectTree();

		QFormLayout *form =
			view.findChild<QFormLayout *>("properties-form");
		QVERIFY(form);
		QCOMPARE(form->rowCount(), 1);
		QLabel *noPropertiesLabel =
			qobject_cast<QLabel *>(form->itemAt(0)->widget());
		QVERIFY(noPropertiesLabel);
		QCOMPARE(noPropertiesLabel->text(),
			 QTStr("Basic.PropertiesWindow.NoProperties"));
	}

	void textProperty()
	{
		OBSPropertiesView view = makeView(
			obs_data_create(), [](obs_properties_t *properties) {
				obs_properties_add_text(properties,
							"test-text-option",
							"Test Option",
							OBS_TEXT_DEFAULT);
			});
		view.dumpObjectTree();

		QFormLayout *form =
			view.findChild<QFormLayout *>("properties-form");
		QVERIFY(form);
		QCOMPARE(form->rowCount(), 1);

		QLabel *propertyLabel =
			qobject_cast<QLabel *>(form->itemAt(0)->widget());
		QVERIFY(propertyLabel);
		QCOMPARE(propertyLabel->text(), QString("Test Option"));

		QLineEdit *propertyEditor =
			qobject_cast<QLineEdit *>(form->itemAt(1)->widget());
		QVERIFY(propertyEditor);
	}

	void textPropertyLoadsValueFromSettings()
	{
		OBSData settings = obs_data_create_from_json(
			R"({"test-option": "initial value"})");
		OBSPropertiesView view =
			makeView(settings, [](obs_properties_t *properties) {
				obs_properties_add_text(properties,
							"test-option",
							"Test Option",
							OBS_TEXT_DEFAULT);
			});
		view.dumpObjectTree();

		QLineEdit *propertyEditor = view.findChild<QLineEdit *>();
		QVERIFY(propertyEditor);
		QCOMPARE(propertyEditor->text(), QString("initial value"));
	}

	void textPropertyReloadsValueFromSettings()
	{
		OBSData settings = obs_data_create_from_json(
			R"({"test-option": "initial value"})");
		OBSPropertiesView view =
			makeView(settings, [](obs_properties_t *properties) {
				obs_properties_add_text(properties,
							"test-option",
							"Test Option",
							OBS_TEXT_DEFAULT);
			});
		view.dumpObjectTree();

		obs_data_set_string(settings, "test-option", "updated value");
		view.ReloadProperties();
		view.dumpObjectTree();

		QLineEdit *propertyEditor = view.findChild<QLineEdit *>();
		QVERIFY(propertyEditor);
		QCOMPARE(propertyEditor->text(), QString("updated value"));
	}

	void editingTextPropertySavesToSettings()
	{
		OBSData settings = obs_data_create_from_json(
			R"({"test-option": "initial value"})");
		OBSPropertiesView view =
			makeView(settings, [](obs_properties_t *properties) {
				obs_properties_add_text(properties,
							"test-option",
							"Test Option",
							OBS_TEXT_DEFAULT);
			});
		view.dumpObjectTree();

		QLineEdit *propertyEditor = view.findChild<QLineEdit *>();
		QVERIFY(propertyEditor);
		propertyEditor->selectAll();
		QTest::keyClicks(propertyEditor, "updated value");
		QCOMPARE(obs_data_get_string(settings, "test-option"),
			 "updated value");
	}

	void cleanup() { viewClosures.clear(); }

private:
	struct ViewClosure {
		virtual ~ViewClosure() = default;
	};

	template<class PropertiesBuilder>
	OBSPropertiesView makeView(OBSData settings,
				   PropertiesBuilder buildProperties)
	{
		struct Closure : public ViewClosure {
			explicit Closure(PropertiesBuilder buildProperties)
				: buildProperties(std::move(buildProperties))
			{
			}

			static obs_properties_t *ReloadCallback(void *obj)
			{
				Closure *closure = static_cast<Closure *>(obj);
				obs_properties_t *properties =
					obs_properties_create();
				closure->buildProperties(properties);
				return properties;
			}

			static void UpdateCallback(void *, obs_data_t *) {}

			PropertiesBuilder buildProperties;
		};
		viewClosures.emplace_back(
			std::make_unique<Closure>(buildProperties));
		return OBSPropertiesView(settings, viewClosures.back().get(),
					 Closure::ReloadCallback,
					 Closure::UpdateCallback);
	}

	std::vector<std::unique_ptr<ViewClosure>> viewClosures;
};

int main(int argc, char *argv[])
{
	QTEST_SET_MAIN_SOURCE_PATH
	OBSApp app(argc, argv, nullptr);
	TestPropertiesView test;
	return QTest::qExec(&test, argc, argv);
}

#include "properties-view-test.moc"
