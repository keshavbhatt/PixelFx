#include "imageinfowidget.h"
#include "ui_imageinfowidget.h"

#include <QImageWriter>
#include <QLabel>

ImageInfoWidget::ImageInfoWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ImageInfoWidget)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose, true);
    this->setWindowTitle(QApplication::applicationName()+" | "+tr("Image Information Dialog"));

    ui->exifMetaList->setAlternatingRowColors(true);
    ui->exifMetaList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

ImageInfoWidget::~ImageInfoWidget()
{
    delete ui;
}


template<typename T>
QString toStringValue(const T &value)
{
    return QString::fromLatin1("%1").arg(value);
}

template<>
QString toStringValue(const QString &value)
{
    return value;
}

template<>
QString toStringValue(const QByteArray &value)
{
    return QString::fromLatin1(value.toHex());
}

template<>
QString toStringValue(const QExifURational &value)
{
    return QString::fromLatin1("%1/%2").arg(value.first).arg(value.second);
}

template<>
QString toStringValue(const QExifSRational &value)
{
    return QString::fromLatin1("%1/%2").arg(value.first).arg(value.second);
}

template<typename T>
QString toStringValue(const QVector<T> &value)
{
    QStringList result;
    for(typename QVector<T>::ConstIterator it = value.begin(), itEnd = value.end(); it != itEnd; ++it)
        result.append(toStringValue(*it));
    return QString::fromLatin1("{%1}").arg(result.join(QString::fromLatin1(", ")));
}

//https://doc.qt.io/archives/qtextended4.4/qexifvalue.html
template<>
QString toStringValue(const QExifValue &value)
{
    const bool isVector = value.count() > 1;
    switch(value.type())
    {
    case QExifValue::Byte:
        return isVector ? toStringValue(value.toByteVector()) : toStringValue(value.toByte());
    case QExifValue::Ascii:
        return toStringValue(value.toString());
    case QExifValue::Short:
        return isVector ? toStringValue(value.toShortVector()) : toStringValue(value.toShort());
    case QExifValue::Long:
        return isVector ? toStringValue(value.toLongVector()) : toStringValue(value.toLong());
    case QExifValue::Rational:
        return isVector ? toStringValue(value.toRationalVector()) : toStringValue(value.toRational());
    case QExifValue::Undefined:
        return toStringValue(value.toString()); //value.toByteArray()
    case QExifValue::SignedLong:
        return isVector ? toStringValue(value.toSignedLongVector()) : toStringValue(value.toSignedLong());
    case QExifValue::SignedRational:
        return isVector ? toStringValue(value.toSignedRationalVector()) : toStringValue(value.toSignedRational());
    default:
        break;
    }
    return QString::fromLatin1("-");
}

QString ImageInfoWidget::MetaDataEntry(QString name,QString val)
{
    return "<b>"+ name +"</b>  :  "+val;
}

//https://doc.qt.io/archives/qtextended4.4/qexifimageheader.html
QString ImageInfoWidget::makeEntry(int tagID, const QExifValue &value)
{
    switch(tagID)
    {
    // ImageTag
    case QExifImageHeader::ImageWidth: // 0x0100
        return MetaDataEntry(QString::fromLatin1("ImageWidth"), toStringValue(value));
    case QExifImageHeader::ImageLength: // 0x0101
        return MetaDataEntry(QString::fromLatin1("ImageLength"), toStringValue(value));
    case QExifImageHeader::BitsPerSample: // 0x0102
        return MetaDataEntry(QString::fromLatin1("BitsPerSample"), toStringValue(value));
    case QExifImageHeader::Compression: // 0x0103
        return MetaDataEntry(QString::fromLatin1("Compression"), toStringValue(value));
    case QExifImageHeader::PhotometricInterpretation: // 0x0106
        return MetaDataEntry(QString::fromLatin1("PhotometricInterpretation"), toStringValue(value));
    case QExifImageHeader::Orientation: // 0x0112
        return MetaDataEntry(QString::fromLatin1("Orientation"), toStringValue(value));
    case QExifImageHeader::SamplesPerPixel: // 0x0115
        return MetaDataEntry(QString::fromLatin1("SamplesPerPixel"), toStringValue(value));
    case QExifImageHeader::PlanarConfiguration: // 0x011C
        return MetaDataEntry(QString::fromLatin1("PlanarConfiguration"), toStringValue(value));
    case QExifImageHeader::YCbCrSubSampling: // 0x0212
        return MetaDataEntry(QString::fromLatin1("YCbCrSubSampling"), toStringValue(value));
    case QExifImageHeader::XResolution: // 0x011A
        return MetaDataEntry(QString::fromLatin1("XResolution"), toStringValue(value));
    case QExifImageHeader::YResolution: // 0x011B
        return MetaDataEntry(QString::fromLatin1("YResolution"), toStringValue(value));
    case QExifImageHeader::ResolutionUnit: // 0x0128
        return MetaDataEntry(QString::fromLatin1("ResolutionUnit"), toStringValue(value));
    case QExifImageHeader::StripOffsets: // 0x0111
        return MetaDataEntry(QString::fromLatin1("StripOffsets"), toStringValue(value));
    case QExifImageHeader::RowsPerStrip: // 0x0116
        return MetaDataEntry(QString::fromLatin1("RowsPerStrip"), toStringValue(value));
    case QExifImageHeader::StripByteCounts: // 0x0117
        return MetaDataEntry(QString::fromLatin1("StripByteCounts"), toStringValue(value));
    case QExifImageHeader::TransferFunction: // 0x012D
        return MetaDataEntry(QString::fromLatin1("TransferFunction"), toStringValue(value));
    case QExifImageHeader::WhitePoint: // 0x013E
        return MetaDataEntry(QString::fromLatin1("WhitePoint"), toStringValue(value));
    case QExifImageHeader::PrimaryChromaciticies: // 0x013F
        return MetaDataEntry(QString::fromLatin1("PrimaryChromaciticies"), toStringValue(value));
    case QExifImageHeader::YCbCrCoefficients: // 0x0211
        return MetaDataEntry(QString::fromLatin1("YCbCrCoefficients"), toStringValue(value));
    case QExifImageHeader::ReferenceBlackWhite: // 0x0214
        return MetaDataEntry(QString::fromLatin1("ReferenceBlackWhite"), toStringValue(value));
    case QExifImageHeader::DateTime: // 0x0132
        return MetaDataEntry(QString::fromLatin1("DateTime"), toStringValue(value));
    case QExifImageHeader::ImageDescription: // 0x010E
        return MetaDataEntry(QString::fromLatin1("ImageDescription"), toStringValue(value));
    case QExifImageHeader::Make: // 0x010F
        return MetaDataEntry(QString::fromLatin1("Make"), toStringValue(value));
    case QExifImageHeader::Model: // 0x0110
        return MetaDataEntry(QString::fromLatin1("Model"), toStringValue(value));
    case QExifImageHeader::Software: // 0x0131
        return MetaDataEntry(QString::fromLatin1("Software"), toStringValue(value));
    case QExifImageHeader::Artist: // 0x013B
        return MetaDataEntry(QString::fromLatin1("Artist"), toStringValue(value));
    case QExifImageHeader::Copyright: // 0x8298
        return MetaDataEntry(QString::fromLatin1("Copyright"), toStringValue(value));
    // ExifExtendedTag
    case QExifImageHeader::ExifVersion: // 0x9000
        return MetaDataEntry(QString::fromLatin1("ExifVersion"), toStringValue(value));
    case QExifImageHeader::FlashPixVersion: // 0xA000
        return MetaDataEntry(QString::fromLatin1("FlashPixVersion"), toStringValue(value));
    case QExifImageHeader::ColorSpace: // 0xA001
        return MetaDataEntry(QString::fromLatin1("ColorSpace"), toStringValue(value));
    case QExifImageHeader::ComponentsConfiguration: // 0x9101
        return MetaDataEntry(QString::fromLatin1("ComponentsConfiguration"), toStringValue(value));
    case QExifImageHeader::CompressedBitsPerPixel: // 0x9102
        return MetaDataEntry(QString::fromLatin1("CompressedBitsPerPixel"), toStringValue(value));
    case QExifImageHeader::PixelXDimension: // 0xA002
        return MetaDataEntry(QString::fromLatin1("PixelXDimension"), toStringValue(value));
    case QExifImageHeader::PixelYDimension: // 0xA003
        return MetaDataEntry(QString::fromLatin1("PixelYDimension"), toStringValue(value));
    case QExifImageHeader::MakerNote: // 0x927C
        return MetaDataEntry(QString::fromLatin1("MakerNote"), toStringValue(value));
    case QExifImageHeader::UserComment: // 0x9286
        return MetaDataEntry(QString::fromLatin1("UserComment"), toStringValue(value));
    case QExifImageHeader::RelatedSoundFile: // 0xA004
        return MetaDataEntry(QString::fromLatin1("RelatedSoundFile"), toStringValue(value));
    case QExifImageHeader::DateTimeOriginal: // 0x9003
        return MetaDataEntry(QString::fromLatin1("DateTimeOriginal"), toStringValue(value));
    case QExifImageHeader::DateTimeDigitized: // 0x9004
        return MetaDataEntry(QString::fromLatin1("DateTimeDigitized"), toStringValue(value));
    case QExifImageHeader::SubSecTime: // 0x9290
        return MetaDataEntry(QString::fromLatin1("SubSecTime"), toStringValue(value));
    case QExifImageHeader::SubSecTimeOriginal: // 0x9291
        return MetaDataEntry(QString::fromLatin1("SubSecTimeOriginal"), toStringValue(value));
    case QExifImageHeader::SubSecTimeDigitized: // 0x9292
        return MetaDataEntry(QString::fromLatin1("SubSecTimeDigitized"), toStringValue(value));
    case QExifImageHeader::ImageUniqueId: // 0xA420
        return MetaDataEntry(QString::fromLatin1("ImageUniqueId"), toStringValue(value));
    case QExifImageHeader::ExposureTime: // 0x829A
        return MetaDataEntry(QString::fromLatin1("ExposureTime"), toStringValue(value));
    case QExifImageHeader::FNumber: // 0x829D
        return MetaDataEntry(QString::fromLatin1("FNumber"), toStringValue(value));
    case QExifImageHeader::ExposureProgram: // 0x8822
        return MetaDataEntry(QString::fromLatin1("ExposureProgram"), toStringValue(value));
    case QExifImageHeader::SpectralSensitivity: // 0x8824
        return MetaDataEntry(QString::fromLatin1("SpectralSensitivity"), toStringValue(value));
    case QExifImageHeader::ISOSpeedRatings: // 0x8827
        return MetaDataEntry(QString::fromLatin1("ISOSpeedRatings"), toStringValue(value));
    case QExifImageHeader::Oecf: // 0x8828
        return MetaDataEntry(QString::fromLatin1("Oecf"), toStringValue(value));
    case QExifImageHeader::ShutterSpeedValue: // 0x9201
        return MetaDataEntry(QString::fromLatin1("ShutterSpeedValue"), toStringValue(value));
    case QExifImageHeader::ApertureValue: // 0x9202
        return MetaDataEntry(QString::fromLatin1("ApertureValue"), toStringValue(value));
    case QExifImageHeader::BrightnessValue: // 0x9203
        return MetaDataEntry(QString::fromLatin1("BrightnessValue"), toStringValue(value));
    case QExifImageHeader::ExposureBiasValue: // 0x9204
        return MetaDataEntry(QString::fromLatin1("ExposureBiasValue"), toStringValue(value));
    case QExifImageHeader::MaxApertureValue: // 0x9205
        return MetaDataEntry(QString::fromLatin1("MaxApertureValue"), toStringValue(value));
    case QExifImageHeader::SubjectDistance: // 0x9206
        return MetaDataEntry(QString::fromLatin1("SubjectDistance"), toStringValue(value));
    case QExifImageHeader::MeteringMode: // 0x9207
        return MetaDataEntry(QString::fromLatin1("MeteringMode"), toStringValue(value));
    case QExifImageHeader::LightSource: // 0x9208
        return MetaDataEntry(QString::fromLatin1("LightSource"), toStringValue(value));
    case QExifImageHeader::Flash: // 0x9209
        return MetaDataEntry(QString::fromLatin1("Flash"), toStringValue(value));
    case QExifImageHeader::FocalLength: // 0x920A
        return MetaDataEntry(QString::fromLatin1("FocalLength"), toStringValue(value));
    case QExifImageHeader::SubjectArea: // 0x9214
        return MetaDataEntry(QString::fromLatin1("SubjectArea"), toStringValue(value));
    case QExifImageHeader::FlashEnergy: // 0xA20B
        return MetaDataEntry(QString::fromLatin1("FlashEnergy"), toStringValue(value));
    case QExifImageHeader::SpatialFrequencyResponse: // 0xA20C
        return MetaDataEntry(QString::fromLatin1("SpatialFrequencyResponse"), toStringValue(value));
    case QExifImageHeader::FocalPlaneXResolution: // 0xA20E
        return MetaDataEntry(QString::fromLatin1("FocalPlaneXResolution"), toStringValue(value));
    case QExifImageHeader::FocalPlaneYResolution: // 0xA20F
        return MetaDataEntry(QString::fromLatin1("FocalPlaneYResolution"), toStringValue(value));
    case QExifImageHeader::FocalPlaneResolutionUnit: // 0xA210
        return MetaDataEntry(QString::fromLatin1("FocalPlaneResolutionUnit"), toStringValue(value));
    case QExifImageHeader::SubjectLocation: // 0xA214
        return MetaDataEntry(QString::fromLatin1("SubjectLocation"), toStringValue(value));
    case QExifImageHeader::ExposureIndex: // 0xA215
        return MetaDataEntry(QString::fromLatin1("ExposureIndex"), toStringValue(value));
    case QExifImageHeader::SensingMethod: // 0xA217
        return MetaDataEntry(QString::fromLatin1("SensingMethod"), toStringValue(value));
    case QExifImageHeader::FileSource: // 0xA300
        return MetaDataEntry(QString::fromLatin1("FileSource"), toStringValue(value));
    case QExifImageHeader::SceneType: // 0xA301
        return MetaDataEntry(QString::fromLatin1("SceneType"), toStringValue(value));
    case QExifImageHeader::CfaPattern: // 0xA302
        return MetaDataEntry(QString::fromLatin1("CfaPattern"), toStringValue(value));
    case QExifImageHeader::CustomRendered: // 0xA401
        return MetaDataEntry(QString::fromLatin1("CustomRendered"), toStringValue(value));
    case QExifImageHeader::ExposureMode: // 0xA402
        return MetaDataEntry(QString::fromLatin1("ExposureMode"), toStringValue(value));
    case QExifImageHeader::WhiteBalance: // 0xA403
        return MetaDataEntry(QString::fromLatin1("WhiteBalance"), toStringValue(value));
    case QExifImageHeader::DigitalZoomRatio: // 0xA404
        return MetaDataEntry(QString::fromLatin1("DigitalZoomRatio"), toStringValue(value));
    case QExifImageHeader::FocalLengthIn35mmFilm: // 0xA405
        return MetaDataEntry(QString::fromLatin1("FocalLengthIn35mmFilm"), toStringValue(value));
    case QExifImageHeader::SceneCaptureType: // 0xA406
        return MetaDataEntry(QString::fromLatin1("SceneCaptureType"), toStringValue(value));
    case QExifImageHeader::GainControl: // 0xA407
        return MetaDataEntry(QString::fromLatin1("GainControl"), toStringValue(value));
    case QExifImageHeader::Contrast: // 0xA408
        return MetaDataEntry(QString::fromLatin1("Contrast"), toStringValue(value));
    case QExifImageHeader::Saturation: // 0xA409
        return MetaDataEntry(QString::fromLatin1("Saturation"), toStringValue(value));
    case QExifImageHeader::Sharpness: // 0xA40A
        return MetaDataEntry(QString::fromLatin1("Sharpness"), toStringValue(value));
    case QExifImageHeader::DeviceSettingDescription: // 0xA40B
        return MetaDataEntry(QString::fromLatin1("DeviceSettingDescription"), toStringValue(value));
    case QExifImageHeader::SubjectDistanceRange: // 0x40C
        return MetaDataEntry(QString::fromLatin1("SubjectDistanceRange"), toStringValue(value));
    // GpsTag
    case QExifImageHeader::GpsVersionId: // 0x0000
        return MetaDataEntry(QString::fromLatin1("GpsVersionId"), toStringValue(value));
    case QExifImageHeader::GpsLatitudeRef: // 0x0001
        return MetaDataEntry(QString::fromLatin1("GpsLatitudeRef"), toStringValue(value));
    case QExifImageHeader::GpsLatitude: // 0x0002
        return MetaDataEntry(QString::fromLatin1("GpsLatitude"), toStringValue(value));
    case QExifImageHeader::GpsLongitudeRef: // 0x0003
        return MetaDataEntry(QString::fromLatin1("GpsLongitudeRef"), toStringValue(value));
    case QExifImageHeader::GpsLongitude: // 0x0004
        return MetaDataEntry(QString::fromLatin1("GpsLongitude"), toStringValue(value));
    case QExifImageHeader::GpsAltitudeRef: // 0x0005
        return MetaDataEntry(QString::fromLatin1("GpsAltitudeRef"), toStringValue(value));
    case QExifImageHeader::GpsAltitude: // 0x0006
        return MetaDataEntry(QString::fromLatin1("GpsAltitude"), toStringValue(value));
    case QExifImageHeader::GpsTimeStamp: // 0x0007
        return MetaDataEntry(QString::fromLatin1("GpsTimeStamp"), toStringValue(value));
    case QExifImageHeader::GpsSatellites: // 0x0008
        return MetaDataEntry(QString::fromLatin1("GpsSatellites"), toStringValue(value));
    case QExifImageHeader::GpsStatus: // 0x0009
        return MetaDataEntry(QString::fromLatin1("GpsStatus"), toStringValue(value));
    case QExifImageHeader::GpsMeasureMode: // 0x000A
        return MetaDataEntry(QString::fromLatin1("GpsMeasureMode"), toStringValue(value));
    case QExifImageHeader::GpsDop: // 0x000B
        return MetaDataEntry(QString::fromLatin1("GpsDop"), toStringValue(value));
    case QExifImageHeader::GpsSpeedRef: // 0x000C
        return MetaDataEntry(QString::fromLatin1("GpsSpeedRef"), toStringValue(value));
    case QExifImageHeader::GpsSpeed: // 0x000D
        return MetaDataEntry(QString::fromLatin1("GpsSpeed"), toStringValue(value));
    case QExifImageHeader::GpsTrackRef: // 0x000E
        return MetaDataEntry(QString::fromLatin1("GpsTrackRef"), toStringValue(value));
    case QExifImageHeader::GpsTrack: // 0x000F
        return MetaDataEntry(QString::fromLatin1("GpsTrack"), toStringValue(value));
    case QExifImageHeader::GpsImageDirectionRef: // 0x0010
        return MetaDataEntry(QString::fromLatin1("GpsImageDirectionRef"), toStringValue(value));
    case QExifImageHeader::GpsImageDirection: // 0x0011
        return MetaDataEntry(QString::fromLatin1("GpsImageDirection"), toStringValue(value));
    case QExifImageHeader::GpsMapDatum: // 0x0012
        return MetaDataEntry(QString::fromLatin1("GpsMapDatum"), toStringValue(value));
    case QExifImageHeader::GpsDestLatitudeRef: // 0x0013
        return MetaDataEntry(QString::fromLatin1("GpsDestLatitudeRef"), toStringValue(value));
    case QExifImageHeader::GpsDestLatitude: // 0x0014
        return MetaDataEntry(QString::fromLatin1("GpsDestLatitude"), toStringValue(value));
    case QExifImageHeader::GpsDestLongitudeRef: // 0x0015
        return MetaDataEntry(QString::fromLatin1("GpsDestLongitudeRef"), toStringValue(value));
    case QExifImageHeader::GpsDestLongitude: // 0x0016
        return MetaDataEntry(QString::fromLatin1("GpsDestLongitude"), toStringValue(value));
    case QExifImageHeader::GpsDestBearingRef: // 0x0017
        return MetaDataEntry(QString::fromLatin1("GpsDestBearingRef"), toStringValue(value));
    case QExifImageHeader::GpsDestBearing: // 0x0018
        return MetaDataEntry(QString::fromLatin1("GpsDestBearing"), toStringValue(value));
    case QExifImageHeader::GpsDestDistanceRef: // 0x0019
        return MetaDataEntry(QString::fromLatin1("GpsDestDistanceRef"), toStringValue(value));
    case QExifImageHeader::GpsDestDistance: // 0x001A
        return MetaDataEntry(QString::fromLatin1("GpsDestDistance"), toStringValue(value));
    case QExifImageHeader::GpsProcessingMethod: // 0x001B
        return MetaDataEntry(QString::fromLatin1("GpsProcessingMethod"), toStringValue(value));
    case QExifImageHeader::GpsAreaInformation: // 0x001C
        return MetaDataEntry(QString::fromLatin1("GpsAreaInformation"), toStringValue(value));
    case QExifImageHeader::GpsDateStamp: // 0x001D
        return MetaDataEntry(QString::fromLatin1("GpsDateStamp"), toStringValue(value));
    case QExifImageHeader::GpsDifferential: // 0x001E
        return MetaDataEntry(QString::fromLatin1("GpsDifferential"), toStringValue(value));
    default:
        break;
    }
    return MetaDataEntry(QString::fromLatin1("Unknown (Dec: %1, Hex: 0x%2)"), toStringValue(value));
}


void ImageInfoWidget::fillMetaData(QString fileName)
{
    basicData.clear();
    imageTagsData.clear();
    extendedTagsData.clear();
    gpsTagsData.clear();

    QExifImageHeader exifHeader(fileName);
    QImageReader reader(fileName);
    basicData.append(MetaDataEntry("File",reader.fileName()));
    basicData.append(MetaDataEntry("Format",reader.format()));
    basicData.append(MetaDataEntry("Size",QString::number(reader.size().width())+" x "+QString::number(reader.size().height())));
    if(reader.textKeys().isEmpty()==false){
        foreach (QString var, reader.textKeys()) {
            basicData.append(MetaDataEntry(var,reader.text(var)));
        }
    }
    if(!basicData.isEmpty())
    {
        ui->exifMetaList->addItem(createHeaderItem(true,"Basic Info",ui->exifMetaList));
        foreach (QString var, basicData) {
            ui->exifMetaList->addItem(createHeaderItem(false,var,ui->exifMetaList));
        }
    }


    const QList<QExifImageHeader::ImageTag> imageTags = exifHeader.imageTags();
    if(!imageTags.empty())
    {
        ui->exifMetaList->addItem(createHeaderItem(true,"Image Tags",ui->exifMetaList));
        for(QList<QExifImageHeader::ImageTag>::ConstIterator it = imageTags.constBegin(); it != imageTags.constEnd(); ++it){
            auto t = makeEntry(*it, exifHeader.value(*it));
            imageTagsData.append(t);
            ui->exifMetaList->addItem(createHeaderItem(false,t,ui->exifMetaList));
        }
    }

    const QList<QExifImageHeader::ExifExtendedTag> extendedTags = exifHeader.extendedTags();
    if(!extendedTags.empty())
    {
        ui->exifMetaList->addItem(createHeaderItem(true,"Other Tags",ui->exifMetaList));
        for(QList<QExifImageHeader::ExifExtendedTag>::ConstIterator it = extendedTags.constBegin(); it != extendedTags.constEnd(); ++it){
            auto t = makeEntry(*it, exifHeader.value(*it));
            extendedTagsData.append(t);
            ui->exifMetaList->addItem(createHeaderItem(false,t,ui->exifMetaList));
        }
    }

    const QList<QExifImageHeader::GpsTag> gpsTags = exifHeader.gpsTags();
    if(!gpsTags.empty())
    {
        ui->exifMetaList->addItem(createHeaderItem(true,"GPS Tags",ui->exifMetaList));
         for(QList<QExifImageHeader::GpsTag>::ConstIterator it = gpsTags.constBegin(); it != gpsTags.constEnd(); ++it){
             auto t = makeEntry(*it, exifHeader.value(*it));
             gpsTagsData.append(t);
             ui->exifMetaList->addItem(createHeaderItem(false,t,ui->exifMetaList));
         }
    }

}

QListWidgetItem* ImageInfoWidget::createHeaderItem(bool header, QString title,QListWidget *listWidget)
{
    QLabel *listTitle = new QLabel(listWidget);
    if(header){
        listTitle->setText("<b>"+title+"</b>");
        //listTitle->setAlignment(Qt::AlignCenter);
        listTitle->setFixedHeight(32);
        listTitle->setStyleSheet("padding:0px,4px,0px,4px;background:palette(highlight)");
    }else{
        listTitle->setStyleSheet("padding:4px,4px;background:transparent");
        listTitle->setText(title);
    }
    QListWidgetItem* item;
    item = new QListWidgetItem(listWidget);
    if(header){
        item->setFlags(Qt::NoItemFlags);
    }else{
        item->setToolTip(title);
    }
    listWidget->setItemWidget(item, listTitle);
    item->setSizeHint(listTitle->minimumSizeHint());

    return item;
}
