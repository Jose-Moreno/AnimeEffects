#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QStackedLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFrame>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QFontMetrics>
#include "util/SelectArgs.h"
#include "gui/ExportDialog.h"


namespace
{

template<class tEdit>
void setMinMaxOptionWidth(tEdit* aEdit)
{
    aEdit->setMaximumWidth(200);
    aEdit->setMinimumWidth(50);
}

}

namespace gui
{
//-------------------------------------------------------------------------------------------------
ExportDialog::ExportDialog(core::Project& aProject, const QString& aPath, QWidget* aParent)
    : EasyDialog(tr("Export Dialog"), aParent)
    , mProject(aProject)
    , mCommonParam()
    , mSize()
    , mFrameMax()
    , mFixAspect(true)
    , mSizeUpdating(false)
{
    mCommonParam.path = aPath;

    mCommonParam.size = mProject.attribute().imageSize();
    XC_ASSERT(mCommonParam.size.width() > 0);
    XC_ASSERT(mCommonParam.size.height() > 0);

    auto time = mProject.currentTimeInfo();
    mCommonParam.frame = util::Range(0, time.frameMax);
    mCommonParam.fps = time.fps;

    mSize = mProject.attribute().imageSize();
    mFrameMax = mProject.currentTimeInfo().frameMax;
}

void ExportDialog::pushSizeBox(QFormLayout& aLayout)
{
    // size
    {
        auto x = new QSpinBox();
        auto y = new QSpinBox();
        auto fix = new QCheckBox();
        x->setRange(1, 32767);
        y->setRange(1, 32767);
        x->setValue(mCommonParam.size.width());
        y->setValue(mCommonParam.size.height());
        setMinMaxOptionWidth(x);
        setMinMaxOptionWidth(y);
        fix->setChecked(mFixAspect);

        this->connect(x, util::SelectArgs<int>::from(&QSpinBox::valueChanged), [=](int aValue)
        {
            if (mSizeUpdating) return;
            mSizeUpdating = true;
            QSize& size = this->mCommonParam.size;
            size.setWidth(aValue);
            if (mFixAspect)
            {
                size.setHeight((int)(mSize.height() * aValue / (double)mSize.width()));
                y->setValue(size.height());
            }
            mSizeUpdating = false;
        });

        this->connect(y, util::SelectArgs<int>::from(&QSpinBox::valueChanged), [=](int aValue)
        {
            if (mSizeUpdating) return;
            mSizeUpdating = true;
            QSize& size = this->mCommonParam.size;
            size.setHeight(aValue);
            if (mFixAspect)
            {
                size.setWidth((int)(mSize.width() * aValue / (double)mSize.height()));
                x->setValue(size.width());
            }
            mSizeUpdating = false;
        });

        this->connect(fix, &QCheckBox::clicked, [=](bool aCheck)
        {
            this->mFixAspect = aCheck;
        });

        aLayout.addRow(tr("image width :"), x);
        aLayout.addRow(tr("image height :"), y);
        aLayout.addRow(tr("fix aspect :"), fix);
    }
}

void ExportDialog::pushFrameBox(QFormLayout& aLayout)
{
    // start frame
    auto start = new QSpinBox();
    start->setRange(0, mFrameMax);
    start->setValue(mCommonParam.frame.min());
    setMinMaxOptionWidth(start);

    // end frame
    auto end = new QSpinBox();
    end->setRange(0, mFrameMax);
    end->setValue(mCommonParam.frame.max());
    setMinMaxOptionWidth(end);

    this->connect(start, &QSpinBox::editingFinished, [=]()
    {
        const int v = start->value();
        this->mCommonParam.frame.setMin(v);
        if (this->mCommonParam.frame.max() < v)
        {
            end->setValue(v);
            this->mCommonParam.frame.setMax(v);
        }
    });

    this->connect(end, &QSpinBox::editingFinished, [=]()
    {
        const int v = end->value();
        this->mCommonParam.frame.setMax(v);
        if (v < this->mCommonParam.frame.min())
        {
            start->setValue(v);
            this->mCommonParam.frame.setMin(v);
        }
    });

    aLayout.addRow(tr("start frame :"), start);
    aLayout.addRow(tr("end frame :"), end);
}

void ExportDialog::pushFpsBox(QFormLayout& aLayout)
{
    auto fps = new QSpinBox();
    fps->setRange(1, 60);
    fps->setValue(mCommonParam.fps);
    setMinMaxOptionWidth(fps);

    this->connect(fps, &QSpinBox::editingFinished, [=]()
    {
        this->mCommonParam.fps = fps->value();
    });

    aLayout.addRow(tr("fps :"), fps);
}

//-------------------------------------------------------------------------------------------------
ImageExportDialog::ImageExportDialog(
        core::Project& aProject, const QString& aDirPath,
        const QString& aSuffix, QWidget* aParent)
    : ExportDialog(aProject, aDirPath, aParent)
    , mImageParam()
{
    {
        QString baseName = QFileInfo(aProject.fileName()).baseName();
        if (baseName.isEmpty()) baseName = "nameless";
        mImageParam.name = baseName + "_export";
        mImageParam.suffix = aSuffix;
    }

    // option
    auto group = new QGroupBox(tr("Parameters"));
    group->setLayout(createImageOption());
    this->setMainWidget(group);

    // button box
    this->setOkCancel();
    this->fixSize();
}

QLayout* ImageExportDialog::createImageOption()
{
    auto form = new QFormLayout();
    //form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setLabelAlignment(Qt::AlignRight);

    // prefix name
    {
        auto name = new QLineEdit();
        setMinMaxOptionWidth(name);
        name->setText(mImageParam.name);

        this->connect(name, &QLineEdit::editingFinished, [=]()
        {
            this->mImageParam.name = name->text();
        });

        form->addRow(tr("prefix name :"), name);
    }

    // quality
    if (mImageParam.suffix == "jpg")
    {
        auto quality = new QSpinBox();
        setMinMaxOptionWidth(quality);
        quality->setMinimum(-1);
        quality->setMaximum(100);
        quality->setValue(mImageParam.quality);

        this->connect(quality, &QSpinBox::editingFinished, [=]()
        {
            this->mImageParam.quality = quality->value();
        });

        form->addRow(tr("quality :"), quality);
    }

    this->pushSizeBox(*form);
    this->pushFrameBox(*form);
    this->pushFpsBox(*form);

    return form;
}

//-------------------------------------------------------------------------------------------------
GifExportDialog::GifExportDialog(
        core::Project& aProject, const QString& aFilePath, QWidget* aParent)
    : ExportDialog(aProject, aFilePath, aParent)
    , mGifParam()
{
    {
        this->commonParam().fps = std::min(this->commonParam().fps, 30);
        mGifParam.optimizePalette = false;
        mGifParam.intermediateBps = 5 * 1000 * 1000;
    }

    // option
    auto group = new QGroupBox(tr("Parameters"));
    group->setLayout(createGifOption());
    this->setMainWidget(group);

    // button box
    this->setOkCancel();
    this->fixSize();
}

QLayout* GifExportDialog::createGifOption()
{
    auto form = new QFormLayout();
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setLabelAlignment(Qt::AlignRight);

    this->pushSizeBox(*form);
    this->pushFrameBox(*form);
    this->pushFpsBox(*form);

    // bit rate
    auto kbps = new QSpinBox();
    {
        setMinMaxOptionWidth(kbps);
        kbps->setRange(1, 100 * 1000);
        kbps->setValue(mGifParam.intermediateBps / 1000);

        this->connect(kbps, &QSpinBox::editingFinished, [=]()
        {
            this->mGifParam.intermediateBps = kbps->value() * 1000;
        });
    }

    // optimize palette
    {
        auto opti = new QCheckBox();
        opti->setChecked(mGifParam.optimizePalette);
        kbps->setEnabled(mGifParam.optimizePalette);

        this->connect(opti, &QCheckBox::clicked, [=](bool aCheck)
        {
            this->mGifParam.optimizePalette = aCheck;
            kbps->setEnabled(aCheck);
        });

        form->addRow(tr("optimize palette :"), opti);
        form->addRow(tr("relay bit rate (Kbps) :"), kbps);
    }

    return form;
}

//-------------------------------------------------------------------------------------------------
VideoExportDialog::VideoExportDialog(
        core::Project& aProject, const QString& aFilePath,
        const ctrl::VideoFormat& aFormat, QWidget* aParent)
    : ExportDialog(aProject, aFilePath, aParent)
    , mVideoParam()
{
    {
        mVideoParam.format = aFormat;
        mVideoParam.bps = 5 * 1000 * 1000;
    }

    // option
    auto group = new QGroupBox(tr("Parameters"));
    group->setLayout(createVideoOption());
    this->setMainWidget(group);

    // button box
    this->setOkCancel();
    this->fixSize();
}

void VideoExportDialog::setColorspaceValidity(QComboBox* aBox, bool aIsValid)
{
    if (!aBox) return;

    aBox->setEnabled(aIsValid);
    if (aIsValid)
    {
        aBox->setItemText(0, "BT.709");
        aBox->setItemText(1, "BT.601");
    }
    else
    {
        aBox->setItemText(0, "");
        aBox->setItemText(1, "");
    }
}

void VideoExportDialog::updatePixelFormat(QComboBox* aBox, const QStringList& aPixfmts)
{
    mVideoParam.pixfmt.clear();

    if (!aBox) return;

    auto valid = !aPixfmts.isEmpty();
    aBox->clear();

    if (valid)
    {
        aBox->addItems(aPixfmts);
        mVideoParam.pixfmt = aPixfmts.at(0);
    }

    aBox->setEnabled(valid);
}

void VideoExportDialog::updateCommentLabel(QLabel* aLabel, bool aGPUEnc)
{
    if (!aLabel) return;

    if (aGPUEnc)
    {
        aLabel->setText(tr("The codec requires a special GPU."));
    }
    else
    {
        aLabel->clear();
    }
}

QLayout* VideoExportDialog::createVideoOption()
{
    auto form = new QFormLayout();
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setLabelAlignment(Qt::AlignRight);

    // comment
    QLabel* comment = new QLabel();
    updateCommentLabel(comment, mVideoParam.format.codecs.at(0).gpuenc);

    // colorspace
    QComboBox* colorBox = nullptr;
    for (auto codec : mVideoParam.format.codecs)
    {
        if (codec.colorspace)
        {
            colorBox = new QComboBox();
            break;
        }
    }
    if (colorBox)
    {
        colorBox->addItems(QStringList() << "" << "");

        this->connect(colorBox, util::SelectArgs<int>::from(&QComboBox::currentIndexChanged), [=]()
        {
            this->mVideoParam.colorIndex = colorBox->currentIndex();
        });

        // initialize enabled
        if (!mVideoParam.format.codecs.isEmpty())
        {
            auto valid = mVideoParam.format.codecs.at(0).colorspace;
            setColorspaceValidity(colorBox, valid);
        }
    }

    // pixel format
    QComboBox* pixfmtBox = new QComboBox();
    // connect
    this->connect(pixfmtBox, util::SelectArgs<int>::from(&QComboBox::currentIndexChanged), [=]()
    {
        this->mVideoParam.pixfmt = pixfmtBox->currentText();
    });
    // initialize enabled
    if (!mVideoParam.format.codecs.isEmpty())
    {
        updatePixelFormat(pixfmtBox, mVideoParam.format.codecs.at(0).pixfmts);
    }

    // codec
    if (!mVideoParam.format.codecs.isEmpty())
    {
        mVideoParam.codecIndex = 0;

        auto codecBox = new QComboBox();

        for (auto codec : mVideoParam.format.codecs)
        {
            QString hints;
            if (codec.lossless || codec.transparent)
            {
                if (codec.lossless)
                {
                    hints += tr("lossless");
                }
                if (codec.transparent)
                {
                    if (!hints.isEmpty()) hints += QString(", ");
                    hints += tr("transparent");
                }
                hints = QString(" (") + hints + ")";
            }
            codecBox->addItem(codec.label + hints);
        }

        this->connect(codecBox, util::SelectArgs<int>::from(&QComboBox::currentIndexChanged), [=]()
        {
            auto index = codecBox->currentIndex();
            this->mVideoParam.codecIndex = index;
            this->setColorspaceValidity(colorBox, mVideoParam.format.codecs.at(index).colorspace);
            this->updatePixelFormat(pixfmtBox, mVideoParam.format.codecs.at(index).pixfmts);
            this->updateCommentLabel(comment, mVideoParam.format.codecs.at(index).gpuenc);
        });

        form->addRow(tr("codec :"), codecBox);
    }

    // push colorspace
    form->addRow(tr("color standard :"), colorBox);
    // push pixel format
    form->addRow(tr("pixel format :"), pixfmtBox);

    this->pushSizeBox(*form);
    this->pushFrameBox(*form);
    this->pushFpsBox(*form);

    // bit rate
    {
        auto kbps = new QSpinBox();
        setMinMaxOptionWidth(kbps);
        kbps->setRange(1, 100 * 1000);
        kbps->setValue(mVideoParam.bps / 1000);

        this->connect(kbps, &QSpinBox::editingFinished, [=]()
        {
            this->mVideoParam.bps = kbps->value() * 1000;
        });

        form->addRow(tr("bit rate (Kbps) :"), kbps);
    }

    // push comment
    {
        auto noteGroup = new QGroupBox("");
        noteGroup->setObjectName("note");

        auto subform = new QVBoxLayout();
        noteGroup->setLayout(subform);
        subform->addWidget(comment);

        form->addRow(noteGroup);
    }

    return form;
}

} // namespace gui

