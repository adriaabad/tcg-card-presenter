#include "tcg-card-dock.h"

#include "tcg-card-source.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <callback/proc.h>
#include <util/bmem.h>

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr const char *DockId = "tcg-card-presenter-dock";

QWidget *dockWidget = nullptr;

struct SourceItem {
	QString name;
};

static bool enumCardSources(void *data, obs_source_t *source)
{
	auto *sources = static_cast<QList<SourceItem> *>(data);
	const char *id = obs_source_get_id(source);

	if (id && strcmp(id, TCG_CARD_SOURCE_ID) == 0) {
		sources->push_back({QString::fromUtf8(obs_source_get_name(source))});
	}

	return true;
}

class TcgCardDock final : public QWidget {
public:
	TcgCardDock()
	{
		auto *root = new QVBoxLayout(this);
		root->setContentsMargins(10, 10, 10, 10);
		root->setSpacing(8);

		gameCombo = new QComboBox(this);
		gameCombo->addItem("Magic: The Gathering", "magic");
		gameCombo->addItem("Pokémon TCG (in development)", "pokemon");
		gameCombo->addItem("Yu-Gi-Oh! (in development)", "yugioh");
		gameCombo->addItem("Disney Lorcana (in development)", "lorcana");
		disableComboItem(gameCombo, 1);
		disableComboItem(gameCombo, 2);
		disableComboItem(gameCombo, 3);
		root->addWidget(gameCombo);

		sourceCombo = new QComboBox(this);
		auto *refreshButton = new QPushButton("Refresh", this);
		auto *createButton = new QPushButton("Create source", this);

		auto *sourceRow = new QHBoxLayout();
		sourceRow->addWidget(sourceCombo, 1);
		sourceRow->addWidget(refreshButton);
		sourceRow->addWidget(createButton);
		root->addLayout(sourceRow);

		searchEdit = new QLineEdit(this);
		searchEdit->setPlaceholderText("Search Magic card in English");
		auto *searchButton = new QPushButton("Search", this);
		auto *searchRow = new QHBoxLayout();
		searchRow->addWidget(searchEdit, 1);
		searchRow->addWidget(searchButton);
		root->addLayout(searchRow);

		cardInfo = new QLabel("Search by English card name or pick a local image.", this);
		cardInfo->setWordWrap(true);
		root->addWidget(cardInfo);

		auto *fileRow = new QHBoxLayout();
		fileEdit = new QLineEdit(this);
		fileEdit->setPlaceholderText("Local card image");
		auto *browseButton = new QPushButton("Browse", this);
		fileRow->addWidget(fileEdit, 1);
		fileRow->addWidget(browseButton);
		root->addLayout(fileRow);

		preview = new QLabel("No image selected", this);
		preview->setAlignment(Qt::AlignCenter);
		preview->setMinimumHeight(220);
		preview->setStyleSheet("QLabel { border: 1px solid rgba(128,128,128,0.45); }");
		root->addWidget(preview);

		auto *buttons = new QHBoxLayout();
		auto *showButton = new QPushButton("Show", this);
		auto *hideButton = new QPushButton("Hide", this);
		buttons->addWidget(showButton);
		buttons->addWidget(hideButton);
		root->addLayout(buttons);

		auto *settingsBox = new QGroupBox("Timing and animation", this);
		auto *settings = new QFormLayout(settingsBox);

		enterMs = makeSpin(0, 10000, 320, " ms");
		holdMs = makeSpin(0, 120000, 3500, " ms");
		exitMs = makeSpin(0, 10000, 240, " ms");
		enterAnimation = makeAnimationCombo();
		exitAnimation = makeAnimationCombo();

		settings->addRow("Enter", enterMs);
		settings->addRow("Visible", holdMs);
		settings->addRow("Exit", exitMs);
		settings->addRow("Enter animation", enterAnimation);
		settings->addRow("Exit animation", exitAnimation);
		root->addWidget(settingsBox);

		status = new QLabel(this);
		status->setWordWrap(true);
		root->addWidget(status);
		root->addStretch();

		auto *credit = new QLabel("debeloperd by adriaabad · v0.1.0", this);
		credit->setAlignment(Qt::AlignCenter);
		credit->setStyleSheet("QLabel { color: rgba(150,150,150,0.82); font-size: 10px; }");
		root->addWidget(credit);

		connect(refreshButton, &QPushButton::clicked, this, [this]() { refreshSources(); });
		connect(createButton, &QPushButton::clicked, this, [this]() { createSource(); });
		connect(searchButton, &QPushButton::clicked, this, [this]() { searchCard(); });
		connect(searchEdit, &QLineEdit::returnPressed, this, [this]() { searchCard(); });
		connect(browseButton, &QPushButton::clicked, this, [this]() { browseImage(); });
		connect(showButton, &QPushButton::clicked, this, [this]() { showCard(); });
		connect(hideButton, &QPushButton::clicked, this, [this]() { callSourceProc("hide_card"); });
		connect(fileEdit, &QLineEdit::textChanged, this, [this]() { updatePreview(); });

		refreshSources();
	}

private:
	QComboBox *sourceCombo = nullptr;
	QComboBox *gameCombo = nullptr;
	QLineEdit *searchEdit = nullptr;
	QLabel *cardInfo = nullptr;
	QLineEdit *fileEdit = nullptr;
	QLabel *preview = nullptr;
	QSpinBox *enterMs = nullptr;
	QSpinBox *holdMs = nullptr;
	QSpinBox *exitMs = nullptr;
	QComboBox *enterAnimation = nullptr;
	QComboBox *exitAnimation = nullptr;
	QLabel *status = nullptr;
	QNetworkAccessManager network;

	static QSpinBox *makeSpin(int min, int max, int value, const char *suffix)
	{
		auto *spin = new QSpinBox();
		spin->setRange(min, max);
		spin->setSingleStep(max > 20000 ? 250 : 50);
		spin->setValue(value);
		spin->setSuffix(suffix);
		return spin;
	}

	static QComboBox *makeAnimationCombo()
	{
		auto *combo = new QComboBox();
		combo->addItem("Fade", TCG_CARD_ANIMATION_FADE);
		combo->addItem("Slide left", TCG_CARD_ANIMATION_SLIDE_LEFT);
		combo->addItem("Slide right", TCG_CARD_ANIMATION_SLIDE_RIGHT);
		combo->addItem("Slide up", TCG_CARD_ANIMATION_SLIDE_UP);
		combo->addItem("Slide down", TCG_CARD_ANIMATION_SLIDE_DOWN);
		combo->addItem("Zoom", TCG_CARD_ANIMATION_ZOOM);
		combo->addItem("Cut", TCG_CARD_ANIMATION_CUT);
		return combo;
	}

	static void disableComboItem(QComboBox *combo, int index)
	{
		auto *model = qobject_cast<QStandardItemModel *>(combo->model());
		if (!model) {
			return;
		}

		QStandardItem *item = model->item(index);
		if (item) {
			item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
		}
	}

	obs_source_t *currentSource() const
	{
		const QByteArray name = sourceCombo->currentText().toUtf8();
		return name.isEmpty() ? nullptr : obs_get_source_by_name(name.constData());
	}

	void refreshSources()
	{
		const QString current = sourceCombo->currentText();
		QList<SourceItem> sources;
		obs_enum_sources(enumCardSources, &sources);

		sourceCombo->clear();
		for (const auto &source : sources) {
			sourceCombo->addItem(source.name);
		}

		const int index = sourceCombo->findText(current);
		if (index >= 0) {
			sourceCombo->setCurrentIndex(index);
		}

		status->setText(sources.empty() ? "Create a TCG Card Presenter source first."
						: "Ready.");
	}

	void createSource()
	{
		obs_source_t *sceneSource = obs_frontend_get_current_scene();
		obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;

		if (!scene) {
			if (sceneSource) {
				obs_source_release(sceneSource);
			}
			status->setText("Select a scene before creating the source.");
			return;
		}

		int suffix = 1;
		QString name;
		do {
			name = suffix == 1 ? "TCG Card Presenter" : QString("TCG Card Presenter %1").arg(suffix);
			obs_source_t *existing = obs_get_source_by_name(name.toUtf8().constData());
			if (!existing) {
				break;
			}
			obs_source_release(existing);
			suffix++;
		} while (suffix < 100);

		obs_data_t *settings = obs_data_create();
		obs_source_t *source = obs_source_create(TCG_CARD_SOURCE_ID, name.toUtf8().constData(), settings, nullptr);
		obs_data_release(settings);

		if (source) {
			obs_scene_add(scene, source);
			obs_source_release(source);
		}

		obs_source_release(sceneSource);
		refreshSources();
		sourceCombo->setCurrentText(name);
		status->setText("Source created in the current scene.");
	}

	void browseImage()
	{
		const QString path = QFileDialog::getOpenFileName(
			this, "Select card image", QString(), "Images (*.png *.jpg *.jpeg *.webp *.gif);;All files (*.*)");

		if (!path.isEmpty()) {
			fileEdit->setText(path);
		}
	}

	void searchCard()
	{
		const QString query = searchEdit->text().trimmed();
		if (query.isEmpty()) {
			status->setText("Type an English card name first.");
			return;
		}

		QUrl url("https://api.scryfall.com/cards/named");
		QUrlQuery params;
		params.addQueryItem("fuzzy", query);
		url.setQuery(params);

		QNetworkRequest request(url);
		request.setRawHeader("Accept", "application/json");
		request.setRawHeader("User-Agent", "tcg-card-presenter-obs/0.1");

		status->setText(QString("Searching Scryfall for %1...").arg(query));
		QNetworkReply *reply = network.get(request);
		connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleCardReply(reply); });
	}

	void handleCardReply(QNetworkReply *reply)
	{
		reply->deleteLater();

		if (reply->error() != QNetworkReply::NoError) {
			status->setText(QString("Search failed: %1").arg(reply->errorString()));
			return;
		}

		const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
		const QJsonObject card = doc.object();
		const QString name = card.value("name").toString();
		QString imageUrl = imageUrlForCard(card);

		if (name.isEmpty() || imageUrl.isEmpty()) {
			status->setText("Scryfall found a card, but no usable image was returned.");
			return;
		}

		cardInfo->setText(QString("%1\n%2 #%3")
					  .arg(name, card.value("set_name").toString(),
					       card.value("collector_number").toString()));
		status->setText(QString("Downloading image for %1...").arg(name));
		downloadCardImage(name, card.value("id").toString(), imageUrl);
	}

	static QString imageUrlForCard(const QJsonObject &card)
	{
		const QJsonObject imageUris = card.value("image_uris").toObject();
		QString url = imageUris.value("large").toString();
		if (url.isEmpty()) {
			url = imageUris.value("normal").toString();
		}

		if (!url.isEmpty()) {
			return url;
		}

		const auto faces = card.value("card_faces").toArray();
		if (faces.isEmpty()) {
			return QString();
		}

		const QJsonObject faceUris = faces.first().toObject().value("image_uris").toObject();
		url = faceUris.value("large").toString();
		return url.isEmpty() ? faceUris.value("normal").toString() : url;
	}

	void downloadCardImage(const QString &cardName, const QString &cardId, const QString &imageUrl)
	{
		QNetworkRequest request{QUrl(imageUrl)};
		request.setRawHeader("User-Agent", "tcg-card-presenter-obs/0.1");

		QNetworkReply *reply = network.get(request);
		connect(reply, &QNetworkReply::finished, this, [this, reply, cardName, cardId]() {
			handleImageReply(reply, cardName, cardId);
		});
	}

	void handleImageReply(QNetworkReply *reply, const QString &cardName, const QString &cardId)
	{
		reply->deleteLater();

		if (reply->error() != QNetworkReply::NoError) {
			status->setText(QString("Image download failed: %1").arg(reply->errorString()));
			return;
		}

		const QByteArray bytes = reply->readAll();
		const QString path = cachePathForCard(cardName, cardId);
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			status->setText(QString("Could not write cache file: %1").arg(path));
			return;
		}

		file.write(bytes);
		file.close();
		fileEdit->setText(QDir::toNativeSeparators(path));
		status->setText(QString("Ready: %1").arg(cardName));
	}

	static QString cachePathForCard(const QString &cardName, const QString &cardId)
	{
		char *rawPath = obs_module_config_path("cache");
		QString cacheDir = rawPath ? QString::fromUtf8(rawPath) : QDir::tempPath() + "/tcg-card-presenter-cache";
		bfree(rawPath);

		QDir().mkpath(cacheDir);

		QString base = cardId.isEmpty() ? cardName : cardId;
		base.replace(QRegularExpression("[^A-Za-z0-9_-]+"), "-");
		base = base.left(96).trimmed();
		if (base.isEmpty()) {
			base = "card";
		}

		return QDir(cacheDir).filePath(base + ".jpg");
	}

	void updatePreview()
	{
		QPixmap image(fileEdit->text());
		if (image.isNull()) {
			preview->setText("No image selected");
			preview->setPixmap(QPixmap());
			return;
		}

		preview->setPixmap(image.scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}

	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		updatePreview();
	}

	void applySettings(obs_source_t *source)
	{
		obs_data_t *settings = obs_source_get_settings(source);
		obs_data_set_string(settings, TCG_CARD_SETTING_CARD_FILE, fileEdit->text().toUtf8().constData());
		obs_data_set_int(settings, TCG_CARD_SETTING_ENTER_MS, enterMs->value());
		obs_data_set_int(settings, TCG_CARD_SETTING_HOLD_MS, holdMs->value());
		obs_data_set_int(settings, TCG_CARD_SETTING_EXIT_MS, exitMs->value());
		obs_data_set_string(settings, TCG_CARD_SETTING_ENTER_ANIMATION,
				    enterAnimation->currentData().toByteArray().constData());
		obs_data_set_string(settings, TCG_CARD_SETTING_EXIT_ANIMATION,
				    exitAnimation->currentData().toByteArray().constData());
		obs_source_update(source, settings);
		obs_data_release(settings);
	}

	void showCard()
	{
		obs_source_t *source = currentSource();
		if (!source) {
			status->setText("No TCG Card Presenter source selected.");
			return;
		}

		applySettings(source);
		const QString sourceName = QString::fromUtf8(obs_source_get_name(source));
		callSourceProc(source, "show_card");
		obs_source_release(source);
		QTimer::singleShot(80, this, [this, sourceName]() {
			obs_source_t *retrySource = obs_get_source_by_name(sourceName.toUtf8().constData());
			if (!retrySource) {
				return;
			}

			callSourceProc(retrySource, "show_card");
			obs_source_release(retrySource);
		});
		status->setText(searchEdit->text().isEmpty() ? "Showing card." : QString("Showing: %1").arg(searchEdit->text()));
	}

	void callSourceProc(const char *name)
	{
		obs_source_t *source = currentSource();
		if (!source) {
			status->setText("No TCG Card Presenter source selected.");
			return;
		}

		callSourceProc(source, name);
		obs_source_release(source);
		status->setText(QString("Sent %1.").arg(name));
	}

	static void callSourceProc(obs_source_t *source, const char *name)
	{
		calldata_t data;
		calldata_init(&data);
		proc_handler_call(obs_source_get_proc_handler(source), name, &data);
		calldata_free(&data);
	}
};

} // namespace

extern "C" void tcg_card_dock_register(void)
{
	if (dockWidget) {
		return;
	}

	dockWidget = new TcgCardDock();
	if (!obs_frontend_add_dock_by_id(DockId, "TCG Card Presenter", dockWidget)) {
		delete dockWidget;
		dockWidget = nullptr;
		blog(LOG_WARNING, "[tcg-card-presenter] could not create dock");
	}
}

extern "C" void tcg_card_dock_unregister(void)
{
	if (!dockWidget) {
		return;
	}

	obs_frontend_remove_dock(DockId);
	dockWidget = nullptr;
}
