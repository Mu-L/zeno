#include "serialize.h"
#include <zeno/utils/logger.h>
#include <zenomodel/include/modeldata.h>
#include <zenomodel/include/modelrole.h>
#include <zenomodel/include/uihelper.h>
#include "util/log.h"
#include "util/apphelper.h"
#include "variantptr.h"
#include "settings/zsettings.h"
#include <QSet>

using namespace JsonHelper;

QSet<QString> lightCameraNodes({
    "CameraEval", "CameraNode", "CihouMayaCameraFov", "ExtractCameraData", "GetAlembicCamera","MakeCamera",
    "LightNode", "BindLight", "ProceduralSky", "HDRSky", "SkyComposer"
    });

std::set<std::string> matNodeNames = {"ShaderFinalize", "ShaderVolume", "ShaderVolumeHomogeneous"};

static QString nameMangling(const QString& prefix, const QString& ident) {
    if (prefix.isEmpty())
        return ident;
    else
        return prefix + "/" + ident;
}

void resolveOutputSocket(
            const QModelIndex& outNodeIdx,
            const QModelIndex& outSockIdx,
            const QString& graphIdPrefix,
            QString& realOutputId,
            QString& realOutputSock,
            RAPIDJSON_WRITER& writer)
{
    const QString& outSock = outSockIdx.data(ROLE_PARAM_NAME).toString();
    const QString& outNodeId = outNodeIdx.data(ROLE_OBJID).toString();
    if (outSockIdx.data(ROLE_PARAM_CLASS) == PARAM_INNER_OUTPUT)
    {
        QModelIndex dictlistIdx = outSockIdx.data(ROLE_PARAM_COREIDX).toModelIndex();
        bool bDict = dictlistIdx.data(ROLE_PARAM_TYPE) == "dict";
        QString _tmpNode = bDict ? "ExtractDict" : "list-what?";
        QString mockNode = UiHelper::generateUuid(_tmpNode);
        mockNode = nameMangling(graphIdPrefix, mockNode);
        AddStringList({"addNode", _tmpNode, mockNode}, writer);

        QString mockSocket = bDict ? "dict" : "list";

        QString dictlistName = dictlistIdx.data(ROLE_PARAM_NAME).toString();
        AddStringList({"addNodeOutput", mockNode, outSock}, writer);

        //add link from source output node    to    mockNode(ExtractDict).
        const QString& mockOutNode = nameMangling(graphIdPrefix, outNodeId);
        AddStringList({"bindNodeInput", mockNode, mockSocket, mockOutNode, dictlistName}, writer);

        realOutputId = mockNode;
        realOutputSock = outSock;
    }
    else
    {
        // normal case:
        const QString &newOutId = nameMangling(graphIdPrefix, outNodeId);
        realOutputId = newOutId;
        realOutputSock = outSock;
    }
}


static void getOptStr(const QString& sockType, QVariant& defl, QString& opStr)
{
    if (sockType != "curve" && defl.canConvert<CURVES_DATA>())
        opStr = "setKeyFrame";
    else if (defl.toString().startsWith("=") || defl.canConvert<UI_VECSTRING>())
    {
        if (defl.canConvert<UI_VECSTRING>()) {
            UI_VECSTRING vec = defl.value<UI_VECSTRING>();
            if (vec.size() != 3) {
                return;
            }

            QString code = "vec3(";
            bool bFormula = false;
            for (int i = 0; i < vec.size(); i++)
            {
                QString text = vec.at(i);
                if (text.startsWith("="))
                {
                    text.replace(0, 1, "");
                    bFormula = true;
                }
                code += text;
                if (i < vec.size() - 1)
                    code += ",";
                else
                    code += ")";
            }
            if (bFormula)
            {
                opStr = "setFormula";
            }
            defl = code;
        }
        else if (sockType == "int" || sockType == "float")
        {
            QString str = defl.toString();
            str.replace(0, 1, "");
            defl = str;
            opStr = "setFormula";
        }
        else
        {
            opStr = "setFormula";
        }
    }
}

static void serializeGraph(IGraphsModel* pGraphsModel, const QModelIndex& subgIdx, QString const &graphIdPrefix, bool bView, RAPIDJSON_WRITER& writer, LAUNCH_PARAM launchParam, bool bNestedSubg = true)
{
    ZASSERT_EXIT(pGraphsModel && subgIdx.isValid());

    rapidjson::Document configDoc;
    if (!launchParam.paramPath.isEmpty())
    {
        QFile file(launchParam.paramPath);
        bool ret = file.open(QIODevice::ReadOnly | QIODevice::Text);
        if (!ret) {
            zeno::log_error("cannot open config file: {} ({})", launchParam.paramPath.toStdString(),
                file.errorString().toStdString());
        }

        QByteArray bytes = file.readAll();
        configDoc.Parse(bytes);

        if (!configDoc.IsObject())
        {
            zeno::log_error("config file is corrupted");
        }
    }
    if (!launchParam.paramBase64.isEmpty()) {
        zeno::log_info("base64:{}", launchParam.paramBase64.toStdString());
        QByteArray base64Encoded = launchParam.paramBase64.toUtf8();
        QByteArray decodedByteArray  = QByteArray::fromBase64(base64Encoded);
        QString decodedString = QString::fromUtf8(decodedByteArray );
        zeno::log_info("json: {}", decodedString.toStdString());
        configDoc.Parse(decodedByteArray );

        if (!configDoc.IsObject()) {
            zeno::log_error("paramsBase64 is corrupted");
        }
    }

    //scan all the nodes in the subgraph.
    for (int i = 0; i < pGraphsModel->itemCount(subgIdx); i++)
    {
        const QModelIndex& idx = pGraphsModel->index(i, subgIdx);
        QString ident = idx.data(ROLE_OBJID).toString();
        ident = nameMangling(graphIdPrefix, ident);
        const QString& name = idx.data(ROLE_OBJNAME).toString();
        //temp: need a node type or flag to mark this case.
        if (name == "Blackboard" || name == "Group") {
            continue;
        }

        if (NO_VERSION_NODE == idx.data(ROLE_NODETYPE))
            continue;

        int opts = idx.data(ROLE_OPTIONS).toInt();
        QString noOnceIdent;
        if (opts & OPT_ONCE) {
            noOnceIdent = ident;
            ident = ident + ":RUNONCE";
        }

        bool bSubgNode = pGraphsModel->IsSubGraphNode(idx);

        INPUT_SOCKETS inputs = idx.data(ROLE_INPUTS).value<INPUT_SOCKETS>();
        OUTPUT_SOCKETS outputs = idx.data(ROLE_OUTPUTS).value<OUTPUT_SOCKETS>();

        if (opts & OPT_MUTE)
        {
            AddStringList({ "addNode", "HelperMute", ident }, writer);
        }
        else
        {
            if (!bSubgNode || !bNestedSubg)
            {
                AddStringList({"addNode", name, ident}, writer);
            }
            else
            {
                AddStringList({"addSubnetNode", name, ident}, writer);
                AddStringList({"pushSubnetScope", ident}, writer);
                const QString& prefix = nameMangling(graphIdPrefix, idx.data(ROLE_OBJID).toString());
                bool _bView = bView && (idx.data(ROLE_OPTIONS).toInt() & OPT_VIEW);
                serializeGraph(pGraphsModel, pGraphsModel->index(name), prefix, _bView, writer, launchParam, true);
                AddStringList({"popSubnetScope", ident}, writer);
            }
        }

        auto outputIt = outputs.begin();

        //sort for inputs and outputs, ensure that the SRC/DST key is the last key to serialize.
        AppHelper::ensureSRCDSTlastKey(inputs, outputs);

        QModelIndexList inputsIndice, paramsIndice, outputsIndice;
        UiHelper::getAllParamsIndex(idx, inputsIndice, paramsIndice, outputsIndice, true);

        for (QModelIndex inSockIdx : inputsIndice)
        {
            bool bCoreParam = inSockIdx.data(ROLE_VPARAM_IS_COREPARAM).toBool();
            QString inputName = inSockIdx.data(ROLE_PARAM_NAME).toString();
            const QString& inSockType = inSockIdx.data(ROLE_PARAM_TYPE).toString();

            if (opts & OPT_MUTE) {
                if (outputIt != outputs.end()) {
                    OUTPUT_SOCKET output = *outputIt++;
                    inputName = output.info.name; // HelperMute forward all inputs to outputs by socket name
                } else {
                    inputName += ":DUMMYDEP";
                }
            }

            //check net label.
            const QString& netlabel = inSockIdx.data(ROLE_PARAM_NETLABEL).toString();
            const PARAM_LINKS& links = inSockIdx.data(ROLE_PARAM_LINKS).value<PARAM_LINKS>();
            ZASSERT_EXIT(netlabel.isEmpty() || links.isEmpty());

            if (!netlabel.isEmpty())
            {
                const QModelIndex& outSockIdx = pGraphsModel->getNetOutput(subgIdx, netlabel);
                const QModelIndex& outIdx = outSockIdx.data(ROLE_NODE_IDX).toModelIndex();
                QString newOutId, outSock;
                // may the output socket is a key socket from a dict param.
                resolveOutputSocket(outIdx, outSockIdx, graphIdPrefix, newOutId, outSock, writer);
                AddStringList({ "bindNodeInput", ident, inputName, newOutId, outSock }, writer);
            }
            else if (links.isEmpty())
            {
                // check whether 
                const int inSockProp = inSockIdx.data(ROLE_PARAM_SOCKPROP).toInt();

                if (inSockProp & SOCKPROP_DICTLIST_PANEL)
                {
                    //check existing links inside the panel.
                    bool bDict = inSockType == "dict";
                    QAbstractItemModel* pKeyObjModel = QVariantPtr<QAbstractItemModel>::asPtr(inSockIdx.data(ROLE_VPARAM_LINK_MODEL));
                    QString mockDictList;
                    int idxWithLink = 0;
                    for (int r = 0; r < pKeyObjModel->rowCount(); r++)
                    {
                        const QModelIndex& keyIdx = pKeyObjModel->index(r, 0);
                        QString keyName = keyIdx.data(ROLE_PARAM_NAME).toString();
                        PARAM_LINKS links = keyIdx.data(ROLE_PARAM_LINKS).value<PARAM_LINKS>();
                        const QString& netlabel = keyIdx.data(ROLE_PARAM_NETLABEL).toString();
                        ZASSERT_EXIT(links.size() <= 1);
                        QModelIndex outSockIdx;
                        QModelIndex outIdx;
                        if (!netlabel.isEmpty())
                        {
                            outSockIdx = pGraphsModel->getNetOutput(subgIdx, netlabel);
                            outIdx = outSockIdx.data(ROLE_NODE_IDX).toModelIndex();
                        }
                        else {
                            for (QModelIndex link : links)
                            {
                                if (link.isValid())
                                {
                                    outIdx = link.data(ROLE_OUTNODE_IDX).toModelIndex();
                                    outSockIdx = link.data(ROLE_OUTSOCK_IDX).toModelIndex();
                                }
                            }
                        }
                        if (outSockIdx.isValid() && outIdx.isValid())
                        {
                            if (!bDict)
                            {
                                //obj number sequence resolve for MakeList.
                                keyName = QString("obj%1").arg(idxWithLink);
                            }
                            QString newOutId, outSock;
                            // may the output socket is a key socket from a dict param.
                            resolveOutputSocket(outIdx, outSockIdx, graphIdPrefix, newOutId, outSock, writer);

                            if (mockDictList.isEmpty())
                            {
                                //create dict or list as a middle node to connect each other.
                                QString _tmpNode = bDict ? "MakeDict" : "MakeList";
                                mockDictList = UiHelper::generateUuid(_tmpNode);
                                mockDictList = nameMangling(graphIdPrefix, mockDictList);
                                AddStringList({ "addNode", _tmpNode, mockDictList }, writer);
                            }
                            if (!bDict)
                            {
                                //new added param `doConcat` at MakeList.
                                AddParams("setNodeParam", mockDictList, "doConcat", 1, "bool", writer);
                            }
                            // add link from outside node to the mock dict/list.
                            AddStringList({ "bindNodeInput", mockDictList, keyName, newOutId, outSock }, writer);
                            idxWithLink++;
                        }
                    }
                    if (!mockDictList.isEmpty())
                    {
                        //add link from mock dict/list to this input socket.
                        AddStringList({"bindNodeInput", ident, inputName, mockDictList, bDict ? "dict" : "list"}, writer);
                    }
                }

                const QString& sockType = inSockIdx.data(ROLE_PARAM_TYPE).toString();
                const FuckQMap<QString, CommandParam>& commandParams = pGraphsModel->commandParams();
                QVariant defl = inSockIdx.data(ROLE_PARAM_VALUE);

                //command params
                const QString& objPath = inSockIdx.data(ROLE_OBJPATH).toString();
                if (commandParams.contains(objPath))
                {
                    if (!launchParam.paramPath.isEmpty() || !launchParam.paramBase64.isEmpty())
                    {
                        const QString& command = commandParams[objPath].name;
                        if (configDoc.HasMember(command.toUtf8()))
                        {
                            const auto& value = configDoc[command.toStdString().c_str()];
                            defl = UiHelper::parseJsonByType(sockType, value, nullptr);
                        }
                    }
                    else if (commandParams[objPath].bIsCommand)
                    {
                        defl = commandParams[objPath].value;
                    }
                }

                QString opStr = "setNodeInput";
                getOptStr(sockType, defl, opStr);
                if (opStr == "setNodeInput") {
                    defl = UiHelper::parseVarByType(sockType, defl, nullptr);
                }
                if (!defl.isNull())
                    AddParams(opStr, ident, inputName, defl, sockType, writer);
            }
            else
            {
                const QModelIndex& link = links[0];

                const QModelIndex& outIdx = link.data(ROLE_OUTNODE_IDX).toModelIndex();
                if (NO_VERSION_NODE == outIdx.data(ROLE_NODETYPE))
                    continue;

                const QModelIndex& outSockIdx = link.data(ROLE_OUTSOCK_IDX).toModelIndex();
                if (SOCKPROP_LEGACY == outSockIdx.data(ROLE_PARAM_SOCKPROP))
                    continue;

                QString newOutId, outSock;
                // may the output socket is a key socket from a dict param.
                resolveOutputSocket(outIdx, outSockIdx, graphIdPrefix, newOutId, outSock, writer);
                AddStringList({"bindNodeInput", ident, inputName, newOutId, outSock}, writer);
            }
        }

        const PARAMS_INFO& params = idx.data(ROLE_PARAMETERS).value<PARAMS_INFO>();
        for (PARAM_INFO param_info : params)
        {
            //todo: validation on param value.
            //bool bValid = UiHelper::validateVariant(param_info.value, param_info.typeDesc);
            //ZASSERT_EXIT(bValid);
            QVariant paramValue;
            QString paramName = param_info.name;
            QString opStr = "setNodeParam";
            getOptStr(param_info.typeDesc, param_info.value, opStr);
            if (opStr == "setNodeParam") {
                const FuckQMap<QString, CommandParam>& commandParams = pGraphsModel->commandParams();
                //command params
                const QString& objPath = param_info.paramPath;
                QString command = commandParams[objPath].name;
                if (!launchParam.paramPath.isEmpty() && commandParams.contains(objPath) && configDoc.HasMember(command.toUtf8()))
                {
                    const auto& value = configDoc[command.toStdString().c_str()];
                    paramValue = UiHelper::parseJsonByType(param_info.typeDesc, value, nullptr);
                }
                else
                {
                    paramValue = UiHelper::parseVarByType(param_info.typeDesc, param_info.value, nullptr);
                }
            }
            else {
                //formula/keyframe
                paramValue = param_info.value;
                paramName += ":";
            }
            if (paramValue.isNull())
                continue;
            AddParams(opStr, ident, paramName, paramValue, param_info.typeDesc, writer);
        }

        if (opts & OPT_ONCE) {
            AddStringList({ "addNode", "HelperOnce", noOnceIdent }, writer);
            for (OUTPUT_SOCKET output : outputs) {
                AddStringList({ "bindNodeInput", noOnceIdent, output.info.name, ident, output.info.name }, writer);
            }

            AddStringList({ "completeNode", ident }, writer);
            ident = noOnceIdent;//must before OPT_VIEW branch
        }

        for (OUTPUT_SOCKET output : outputs) {
            //the output key of the dict has not descripted by the core, need to add it manually.
            if (output.info.sockProp & SOCKPROP_EDITABLE) {
                AddStringList({"addNodeOutput", ident, output.info.name}, writer);
            }
        }

        QVariant varDataChanged = idx.data(ROLE_NODE_DATACHANGED);
        if (varDataChanged.isValid() && varDataChanged.toBool())
        {
            AddStringList({"markNodeChanged", ident}, writer);
        }

        AddStringList({ "completeNode", ident }, writer);

		if (bView && (opts & OPT_VIEW))
        {
            if (name == "SubOutput")
            {
                auto viewerIdent = ident + ":TOVIEW";
                AddStringList({"addNode", "ToView", viewerIdent}, writer);
                AddStringList({"bindNodeInput", viewerIdent, "object", ident, "_OUT_port"}, writer);
                bool isStatic = opts & OPT_ONCE;
                AddVariantList({"setNodeInput", viewerIdent, "isStatic", isStatic}, "int", writer);
                AddStringList({"completeNode", viewerIdent}, writer);
            }
            else
            {
                if ((launchParam.runtype == RunLightCamera && !lightCameraNodes.contains(name) || 
                    launchParam.runtype == RunMaterial && matNodeNames.count(name.toStdString())==0) && !pGraphsModel->IsSubGraphNode(idx) ||
                    launchParam.runtype == RunMatrix && name != "SetToMatrix")
                {
                    continue;
                }
                for (OUTPUT_SOCKET output : outputs)
                {
                    //if (output.info.name == "DST" && outputs.size() > 1)
                        //continue;
                    auto viewerIdent = ident + ":TOVIEW";
                    AddStringList({"addNode", "ToView", viewerIdent}, writer);
                    AddStringList({"bindNodeInput", viewerIdent, "object", ident, output.info.name}, writer);
                    bool isStatic = opts & OPT_ONCE;
                    AddVariantList({"setNodeInput", viewerIdent, "isStatic", isStatic}, "int", writer);

                    if (name == "Stamp") {
                        //stamp节点要特殊处理，控制zencache是否导出
                        auto iterParam = params.find("mode");
                        if (iterParam != params.end()) {
                            QString mode = iterParam.value().value.toString();
                            AddVariantList({ "setNodeParam", viewerIdent, "mode", mode }, "string", writer);
                        }
                        iterParam = params.find("name");
                        if (iterParam != params.end()) {
                            QString name = iterParam.value().value.toString();
                            AddVariantList({ "setNodeParam", viewerIdent, "name", name }, "string", writer);
                        }
                    }

                    AddStringList({"completeNode", viewerIdent}, writer);
                    break;  //current node is not a subgraph node, so only one output is needed to view this obj.
                }
            }

            if (lightCameraNodes.contains(name)) {
                AddStringList({ "objRunType", ident, "lightCamera"}, writer);
            } else if ((matNodeNames.count(name.toStdString()) == 1) || pGraphsModel->IsSubGraphNode(idx)) {
                AddStringList({ "objRunType", ident, "material"}, writer);
            } else if (name == "SetToMatrix") {
                AddStringList({ "objRunType", ident, "matrix"}, writer);
            } else {
                AddStringList({ "objRunType", ident, "normal"}, writer);
            }
        }
    }
}

void serializeScene(IGraphsModel* pModel, RAPIDJSON_WRITER& writer, LAUNCH_PARAM param)
{
    serializeGraph(pModel, pModel->index("main"), "", true, writer, param, true);
}

static void serializeSceneOneGraph(IGraphsModel* pModel, RAPIDJSON_WRITER& writer, QString subgName)
{
    LAUNCH_PARAM param;
    serializeGraph(pModel, pModel->index(subgName), "", true, writer, param, false);
}

static void appendSerializedCharArray(QString &res, const char *buf, size_t len) {
    for (auto p = buf; p < buf + len; p++) {
        res.append(QString::number((int)(uint8_t)*p));
        res.append(',');
    }
    res.append('0');
}

QString serializeSceneCpp(IGraphsModel* pModel)
{

    auto *model = pModel;
    QString res = R"RAW(/* auto generated from: )RAW";
    res.append(model->filePath());
    res.append(R"RAW( */
#include <zeno/extra/ISubgraphNode.h>
#include <zeno/zeno.h>
namespace {
)RAW");

    decltype(auto) descs = model->descriptors();
    for (int i = 0; i < model->rowCount(); i++) {
        auto key = model->index(i, 0).data(ROLE_OBJNAME).toString();
        if (key == "main") continue;
        if (!descs.contains(key)) {
            zeno::log_warn("cannot find subgraph `{}` in descriptors table", key.toStdString());
            continue;
        }
        auto const &desc = descs[key];

        res.append(R"RAW(
struct )RAW");
        res.append(key);
        res.append(R"RAW( final : zeno::ISerialSubgraphNode {
    static inline const unsigned char mydata[] = {)RAW");
            rapidjson::StringBuffer s;
            RAPIDJSON_WRITER writer(s);
            {
                JsonArrayBatch batch(writer);
                serializeSceneOneGraph(pModel, writer, key);
            }
            auto subgJson = s.GetString();
            auto subgJsonLen = s.GetLength();
        ZENO_P(std::string(subgJson, subgJsonLen));
        appendSerializedCharArray(res, subgJson, subgJsonLen);
        res.append(R"RAW(};

    virtual const char *get_subgraph_json() override {
        return (const char *)mydata;
    }
};

ZENO_DEFNODE()RAW");

        res.append(key);
        res.append(R"RAW()({
    {)RAW");
        for (auto const &[_, entry] : desc.inputs) {
            if (entry.info.name == "SRC") continue;
            res.append(R"RAW({")RAW");
            res.append(entry.info.type);
            res.append(R"RAW(", ")RAW");
            res.append(entry.info.name);
            res.append(R"RAW(", ")RAW");
            res.append(UiHelper::variantToString(entry.info.defaultValue));
            res.append(R"RAW("}, )RAW");
        }
        res.append(R"RAW(},
    {)RAW");
        for (auto const &[_, entry] : desc.outputs) {
            if (entry.info.name == "DST") continue;
            res.append(R"RAW({")RAW");
            res.append(R"RAW({")RAW");
            res.append(entry.info.type);
            res.append(R"RAW(", ")RAW");
            res.append(entry.info.name);
            res.append(R"RAW(", ")RAW");
            res.append(UiHelper::variantToString(entry.info.defaultValue));
            res.append(R"RAW("}, )RAW");
        }
        res.append(R"RAW(},
    {)RAW");
        for (auto const &entry : desc.params) {
            res.append(R"RAW({")RAW");
            res.append(entry.typeDesc);
            res.append(R"RAW(", ")RAW");
            res.append(entry.name);
            res.append(R"RAW(", ")RAW");
            res.append(UiHelper::variantToString(entry.defaultValue));
            res.append(R"RAW("}, )RAW");
        }
        res.append(R"RAW(},
    {)RAW");
        for (auto const &category : desc.categories) {
            res.append(R"RAW(")RAW");
            res.append(category);
            res.append(R"RAW(", )RAW");
        }
        res.append(R"RAW(},
});
)RAW");
    }
    res.append(R"RAW(
}
)RAW");
    return res;
}
