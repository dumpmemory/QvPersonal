#include "SubscriptionAdapter.hpp"

#include "BuiltinSubscriptionAdapter.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSslKey>
#include <QUrl>
#include <QUrlQuery>

const inline QStringList SplitLines(const QString &_string)
{
    return _string.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
}

QString SafeBase64Decode(QString string)
{
    QByteArray ba = string.replace(QChar('-'), QChar('+')).replace(QChar('_'), QChar('/')).toUtf8();
    return QByteArray::fromBase64(ba, QByteArray::Base64Option::OmitTrailingEquals);
}

QString SafeBase64Encode(const QString &string)
{
    QString base64 = string.toUtf8().toBase64();
    return base64.replace(QChar('+'), QChar('-')).replace(QChar('/'), QChar('_'));
}

// Simple Base64 Decoder
SubscriptionResult SimpleBase64Decoder::DecodeSubscription(const QByteArray &data) const
{
    auto source = QString::fromUtf8(data).trimmed();
    const auto resultList = source.contains(QStringLiteral("://")) ? source : SafeBase64Decode(source);
    //
    SubscriptionResult result;
    result.links = SplitLines(resultList);
    return result;
}

// SIP008 Decoder
SubscriptionResult SIP008Decoder::DecodeSubscription(const QByteArray &data) const
{
    const auto root = QJsonDocument::fromJson(data).object();
    //
    // const auto version = root["version"].toString();
    // const auto username = root["username"].toString();
    // const auto user_uuid = root["user_uuid"].toString();
    const auto servers = root[QStringLiteral("servers")].toArray();

    // ss://Y2hhY2hhMjAtaWV0Zi1wb2x5MTMwNTpwYXNzQGhvc3Q6MTIzNA/?plugin=plugin%3Bopt#sssip003

    SubscriptionResult result;
    for (const auto &servVal : servers)
    {
        const auto serverObj = servVal.toObject();
#define GetVal(x) const auto x = serverObj[u## #x##_qs].toString()
        GetVal(server);
        GetVal(password);
        GetVal(method);
        GetVal(plugin);
        GetVal(plugin_opts);
        GetVal(remarks);
        // GetVal(id);
#undef GetVal

        const auto server_port = serverObj[QStringLiteral("server_port")].toInt();
        bool isSIP003 = !plugin.isEmpty();
        const auto userInfo = SafeBase64Encode(method + QStringLiteral(":") + password);
        //
        QUrl link;
        link.setScheme(QStringLiteral("ss"));
        link.setUserInfo(userInfo);
        link.setHost(server);
        link.setPort(server_port);
        link.setFragment(remarks);
        if (isSIP003)
        {
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("plugin"), QUrl::toPercentEncoding(plugin + QStringLiteral(";") + plugin_opts));
            link.setQuery(q);
        }
        result.links << link.toString(QUrl::FullyEncoded);
    }
    return result;
}

// OOCv1 Decoder
SubscriptionResult OOCProvider::FetchDecodeSubscription(const SubscriptionProviderOptions &options) const
{

    const auto url = u"%1/%2/ooc/v%3/%4"_qs.arg(options.value(u"baseUrl"_qs).toString(),               //
                                                options.value(u"secret"_qs).toString(),                //
                                                QString::number(options.value(u"version"_qs).toInt()), //
                                                options.value(u"userId"_qs).toString());

    const auto pinnedCertChecker = [](QNetworkReply *reply)
    {
        QSslCertificate cert = reply->sslConfiguration().peerCertificate();
        QString serverHash = QCryptographicHash::hash(cert.publicKey().toDer(), QCryptographicHash::Sha256).toHex();
        Q_UNUSED(serverHash);
    };

    const auto &[err, errorString, data] = InternalSubscriptionSupportPlugin::NetworkRequestHelper()->Get(url, pinnedCertChecker);

    return {};
}
