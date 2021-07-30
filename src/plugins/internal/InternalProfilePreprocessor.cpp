#include "InternalProfilePreprocessor.hpp"

#include "Qv2rayApplication.hpp"
#include "Qv2rayBase/Profile/ProfileManager.hpp"
#include "QvPlugin/Utils/QJsonIO.hpp"

constexpr auto DNS_INTERCEPTION_OUTBOUND_TAG = "dns-out";
constexpr auto DEFAULT_FREEDOM_OUTBOUND_TAG = "direct";
constexpr auto DEFAULT_BLACKHOLE_OUTBOUND_TAG = "blackhole";

constexpr auto DEFAULT_DokodemoDoor_IPV4_TAG = "tproxy-in-1";
constexpr auto DEFAULT_DokodemoDoor_IPV6_TAG = "tproxy-in-2";

constexpr auto DEFAULT_SOCKS_IPV4_TAG = "socks-in-1";
constexpr auto DEFAULT_SOCKS_IPV6_TAG = "socks-in-2";

constexpr auto DEFAULT_HTTP_IPV4_TAG = "http-in-1";
constexpr auto DEFAULT_HTTP_IPV6_TAG = "http-in-2";

enum RuleType
{
    RULE_DOMAIN,
    RULE_IP
};

template<RuleType t>
inline RuleObject GenerateSingleRouteRule(const QStringList &rules, const QString &outboundTag)
{
    RuleObject r;
    r.outboundTag = outboundTag;
    if constexpr (t == RULE_DOMAIN)
        r.targetDomains = rules;
    else
        r.targetIPs = rules;
    return r;
}

// -------------------------- BEGIN CONFIG GENERATIONS
void ProcessRoutes(RoutingObject &root, bool ForceDirectConnection, bool bypassCN, bool bypassLAN, const QString &outTag, const RouteMatrixConfig &routeConfig)
{
    root.extraOptions.insert(QStringLiteral("domainStrategy"), *routeConfig.domainStrategy);
    root.extraOptions.insert(QStringLiteral("domainMatcher"), *routeConfig.domainMatcher);
    //
    // For Rules list
    QList<RuleObject> newRulesList;
    if (bypassLAN)
        newRulesList << GenerateSingleRouteRule<RULE_IP>({ "geoip:private" }, DEFAULT_FREEDOM_OUTBOUND_TAG);

    if (ForceDirectConnection)
    {
        // This is added to disable all proxies, as a alternative influence of #64
        newRulesList << GenerateSingleRouteRule<RULE_DOMAIN>({ "regexp:.*" }, DEFAULT_FREEDOM_OUTBOUND_TAG);
        newRulesList << GenerateSingleRouteRule<RULE_IP>({ "0.0.0.0/0" }, DEFAULT_FREEDOM_OUTBOUND_TAG);
        newRulesList << GenerateSingleRouteRule<RULE_IP>({ "::/0" }, DEFAULT_FREEDOM_OUTBOUND_TAG);
    }
    else
    {
        // Blocked.
        if (!routeConfig.ips->block->isEmpty())
            newRulesList << GenerateSingleRouteRule<RULE_IP>(routeConfig.ips->block, DEFAULT_BLACKHOLE_OUTBOUND_TAG);
        if (!routeConfig.domains->block->isEmpty())
            newRulesList << GenerateSingleRouteRule<RULE_DOMAIN>(routeConfig.domains->block, DEFAULT_BLACKHOLE_OUTBOUND_TAG);

        // Proxied
        if (!routeConfig.ips->proxy->isEmpty())
            newRulesList << GenerateSingleRouteRule<RULE_IP>(routeConfig.ips->proxy, outTag);
        if (!routeConfig.domains->proxy->isEmpty())
            newRulesList << GenerateSingleRouteRule<RULE_DOMAIN>(routeConfig.domains->proxy, outTag);

        // Directed
        if (!routeConfig.ips->direct->isEmpty())
            newRulesList << GenerateSingleRouteRule<RULE_IP>(routeConfig.ips->direct, DEFAULT_FREEDOM_OUTBOUND_TAG);
        if (!routeConfig.domains->direct->isEmpty())
            newRulesList << GenerateSingleRouteRule<RULE_DOMAIN>(routeConfig.domains->direct, DEFAULT_FREEDOM_OUTBOUND_TAG);

        // Check if CN needs proxy, or direct.
        if (bypassCN)
        {
            // No proxy agains CN addresses.
            newRulesList << GenerateSingleRouteRule<RULE_IP>({ "geoip:cn" }, DEFAULT_FREEDOM_OUTBOUND_TAG);
            newRulesList << GenerateSingleRouteRule<RULE_DOMAIN>({ "geosite:cn" }, DEFAULT_FREEDOM_OUTBOUND_TAG);
        }
    }

    newRulesList << root.rules;
    root.rules = newRulesList;
}

ProfileContent InternalProfilePreprocessor::PreprocessProfile(const ProfileContent &p)
{
    // For "complex" profiles.
    const auto needGeneration = p.inbounds.isEmpty() && p.routing.rules.isEmpty() && p.outbounds.size() == 1;

    if (!needGeneration)
        return p;

    auto result = p;

    bool hasAddr1 = !GlobalConfig->inboundConfig->ListenAddress1->isEmpty();
    bool hasAddr2 = !GlobalConfig->inboundConfig->ListenAddress2->isEmpty();

#define AddInbound(PROTOCOL, _protocol, ...)                                                                                                                             \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (GlobalConfig->inboundConfig->Has##PROTOCOL)                                                                                                                  \
        {                                                                                                                                                                \
            InboundObject in;                                                                                                                                            \
            in.inboundSettings.protocol = QStringLiteral(_protocol);                                                                                                     \
            GlobalConfig->inboundConfig->PROTOCOL##Config->Propagate(in);                                                                                                \
            if (hasAddr1)                                                                                                                                                \
            {                                                                                                                                                            \
                in.name = QString::fromUtf8(DEFAULT_##PROTOCOL##_IPV4_TAG);                                                                                              \
                in.inboundSettings.address = GlobalConfig->inboundConfig->ListenAddress1;                                                                                \
                __VA_ARGS__;                                                                                                                                             \
                result.inbounds << in;                                                                                                                                   \
            }                                                                                                                                                            \
            if (hasAddr2)                                                                                                                                                \
            {                                                                                                                                                            \
                in.name = QString::fromUtf8(DEFAULT_##PROTOCOL##_IPV6_TAG);                                                                                              \
                in.inboundSettings.address = GlobalConfig->inboundConfig->ListenAddress2;                                                                                \
                __VA_ARGS__;                                                                                                                                             \
                result.inbounds << in;                                                                                                                                   \
            }                                                                                                                                                            \
        }                                                                                                                                                                \
    } while (false)

    const auto dokoMode = [](auto m)
    {
        switch (m)
        {
            case Qv2ray::Models::DokodemoDoorInboundConfig::TPROXY: return QStringLiteral("tproxy");
            case Qv2ray::Models::DokodemoDoorInboundConfig::REDIRECT: return QStringLiteral("redirect");
        }
        return QStringLiteral("redirect");
    }(GlobalConfig->inboundConfig->DokodemoDoorConfig->WorkingMode);

    AddInbound(HTTP, "http", {});
    AddInbound(SOCKS, "socks", {});
    AddInbound(DokodemoDoor, "dokodemo-door", in.inboundSettings.streamSettings[QStringLiteral("sockopt")] = QJsonObject{ { QStringLiteral("tproxy"), dokoMode } });

    const auto routeMatrixConfig = RouteMatrixConfig::fromJson(p.routing.extraOptions[RouteMatrixConfig::EXTRA_OPTIONS_ID].toObject());

    ProcessRoutes(result.routing,                                        //
                  GlobalConfig->connectionConfig->ForceDirectConnection, //
                  GlobalConfig->connectionConfig->BypassCN,              //
                  GlobalConfig->connectionConfig->BypassLAN,             //
                  result.outbounds.first().name,                         //
                  routeMatrixConfig);

    if (GlobalConfig->connectionConfig->BypassBittorrent)
    {
        RuleObject r;
        r.protocols.append(QStringLiteral("bittorrent"));
        r.outboundTag = QString::fromUtf8(DEFAULT_FREEDOM_OUTBOUND_TAG);
        result.routing.rules.prepend(r);
    }

    if (GlobalConfig->connectionConfig->DNSInterception)
    {
        QStringList dnsRuleInboundTags;
        if (GlobalConfig->inboundConfig->HasDokodemoDoor)
        {
            if (hasAddr1)
                dnsRuleInboundTags.append(QString::fromUtf8(DEFAULT_DokodemoDoor_IPV4_TAG));
            if (hasAddr2)
                dnsRuleInboundTags.append(QString::fromUtf8(DEFAULT_DokodemoDoor_IPV6_TAG));
        }

        if (GlobalConfig->inboundConfig->HasSOCKS)
        {
            if (hasAddr1)
                dnsRuleInboundTags.append(QString::fromUtf8(DEFAULT_SOCKS_IPV4_TAG));
            if (hasAddr2)
                dnsRuleInboundTags.append(QString::fromUtf8(DEFAULT_SOCKS_IPV6_TAG));
        }

        // If no UDP inbound, then DNS outbound is useless.
        if (!dnsRuleInboundTags.isEmpty())
        {
            IOConnectionSettings _dns;
            _dns.protocol = QStringLiteral("dns");
            auto DNSOutbound = OutboundObject(_dns);
            DNSOutbound.name = QString::fromUtf8(DNS_INTERCEPTION_OUTBOUND_TAG);

            RuleObject DNSRule;
            DNSRule.targetPort = 53;
            DNSRule.inboundTags = dnsRuleInboundTags;
            DNSRule.outboundTag = QString::fromUtf8(DNS_INTERCEPTION_OUTBOUND_TAG);

            result.outbounds << DNSOutbound;
            result.routing.rules.prepend(DNSRule);
        }
    }
    {
        // Generate Blackhole
        OutboundObject black{ IOConnectionSettings{ QStringLiteral("blackhole"), QStringLiteral("0.0.0.0"), 0 } };
        black.name = QString::fromUtf8(DEFAULT_BLACKHOLE_OUTBOUND_TAG);
        result.outbounds << black;

        // Generate Freedom
        OutboundObject freedom{ IOConnectionSettings{ QStringLiteral("freedom"), QStringLiteral("0.0.0.0"), 0 } };
        freedom.name = QString::fromUtf8(DEFAULT_FREEDOM_OUTBOUND_TAG);
        if (GlobalConfig->connectionConfig->UseDirectOutboundAsPrimary)
            result.outbounds.prepend(freedom);
        else
            result.outbounds.append(freedom);
    }

    return result;
}
