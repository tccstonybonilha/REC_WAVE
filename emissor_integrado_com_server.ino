#include <WiFi.h>              // biblioteca para conectar o ESP32 ao WiFi
#include <WebServer.h>         // biblioteca para criar o servidor HTTP
#include <ArduinoJson.h>       // biblioteca para montar/ler JSON
#include "soc/soc.h"           // acesso a registradores internos do chip
#include "soc/rtc_cntl_reg.h"  // registrador usado para desligar o brownout detector
#include <vector>              // usado para guardar a lista de mensagens

// dados da rede WiFi
const char* ssid     = "Welinton";  // nome da rede
const char* password = "W080601n";  // senha da rede

// servidor HTTP escutando na porta 80
WebServer server(80);

// pino do buzzer usado para transmitir o texto por frequência
const int BUZZER_PIN = 15;

// frequências do protocolo de transmissão
const int FREQUENCIA_CONTROLE = 4000; // marca início e fim da transmissão
const int FREQUENCIA_BIT_0    = 3400; // bit solto final = 0
const int FREQUENCIA_BIT_1    = 3700; // bit solto final = 1

// estrutura que representa uma mensagem do chat
struct ChatMessage {
  String text;  // texto da mensagem
  String time;  // horário (uptime) em que foi registrada
  String type;  // "in" = recebida, "out" = enviada
};

std::vector<ChatMessage> messages;      // lista com todas as mensagens em memória
unsigned int msgSentCount = 0;          // contador de mensagens enviadas
unsigned int msgReceivedCount = 0;      // contador de mensagens recebidas
const size_t MAX_MESSAGES = 100;        // limite máximo de mensagens guardadas

// configurações ajustáveis do dispositivo
String cfgDeviceName = "REC-WAVE 01";  // nome exibido no painel
bool   cfgDarkMode   = true;           // se o tema escuro está ativo
String cfgLanguage   = "Português";    // idioma da interface

bool restartPending = false;  // sinaliza que um reinício foi solicitado
unsigned long restartAt = 0;  // instante (millis) em que deve reiniciar

// página HTML/CSS/JS completa, guardada na memória de programa (PROGMEM)
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>REC-WAVE | Painel de Controle</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
    :root {
        --bg-dark: #0a0e12;
        --bg-card: #12181f;
        --bg-input: #1a222b;
        --text-main: #e0e0e0;
        --text-muted: #8a96a3;
        --neon-green: #39ff14;
        --dark-green: #004d00;
        --accent-green: #1a5c1a;
        --danger: #ff4444;
        --border-color: #232d38;
        --transition: all 0.3s ease;
    }
    body.light-theme {
        --bg-dark: #eef1f5;
        --bg-card: #ffffff;
        --bg-input: #f0f2f5;
        --text-main: #1b222a;
        --text-muted: #5a6472;
        --neon-green: #17a344;
        --dark-green: #d7f5df;
        --accent-green: #1f8a44;
        --danger: #d9333f;
        --border-color: #dde1e6;
    }
    * { margin: 0; padding: 0; box-sizing: border-box;
        font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }
    body { background-color: var(--bg-dark); color: var(--text-main);
        height: 100vh; display: flex; justify-content: center;
        align-items: center; overflow: hidden; transition: background-color 0.3s ease, color 0.3s ease; }
    .app-container { width: 100%; max-width: 500px; height: 100vh;
        background-color: var(--bg-dark); display: flex; flex-direction: column;
        border-left: 1px solid var(--border-color);
        border-right: 1px solid var(--border-color); }
    .main-header { padding: 15px 20px; display: flex;
        justify-content: space-between; align-items: center;
        border-bottom: 1px solid var(--border-color); }
    .logo-container { display: flex; align-items: center; gap: 10px; }
    .logo-icon { color: var(--neon-green); font-size: 1.5rem; }
    .logo-text { font-size: 1.2rem; font-weight: 700; letter-spacing: 1px; }
    .header-status { display: flex; align-items: center; gap: 15px; color: var(--text-muted); }
    .tab-navigation { display: flex; padding: 10px 10px 0 10px; gap: 5px;
        border-bottom: 1px solid var(--border-color); overflow-x: auto; }
    .tab-btn { flex: 1; background: none; border: none; color: var(--text-muted);
        padding: 12px 5px; cursor: pointer; font-size: 0.85rem;
        display: flex; flex-direction: column; align-items: center;
        gap: 5px; transition: var(--transition);
        border-bottom: 3px solid transparent; white-space: nowrap; }
    .tab-btn i { font-size: 1.1rem; }
    .tab-btn.active { color: var(--neon-green);
        border-bottom-color: var(--neon-green); }
    .content-area { flex: 1; overflow-y: auto; padding: 20px; }
    .tab-content { display: none; animation: fadeIn 0.4s ease; }
    .tab-content.active { display: block; }
    @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); }
        to { opacity: 1; transform: translateY(0); } }
    .section-header { display: flex; justify-content: space-between;
        align-items: center; margin-bottom: 20px; }
    .section-header h2 { font-size: 1.1rem; font-weight: 600; }
    .status-indicator { font-size: 0.8rem; display: flex; align-items: center; gap: 5px; }
    .status-indicator::before { content: ''; width: 8px; height: 8px; border-radius: 50%; }
    .status-indicator.online { color: var(--neon-green); }
    .status-indicator.online::before { background-color: var(--neon-green);
        box-shadow: 0 0 5px var(--neon-green); }
    .chat-window { height: 300px; background-color: var(--bg-card);
        border-radius: 12px; padding: 15px; overflow-y: auto;
        display: flex; flex-direction: column; gap: 15px;
        margin-bottom: 15px; border: 1px solid var(--border-color); }
    .message { max-width: 80%; padding: 10px 14px; border-radius: 15px;
        font-size: 0.9rem; position: relative; }
    .message.received { align-self: flex-start; background-color: #232d38;
        border-bottom-left-radius: 2px; }
    body.light-theme .message.received { background-color: #e7ebf0; color: #1b222a; }
    .message.sent { align-self: flex-end; background-color: var(--accent-green);
        border-bottom-right-radius: 2px; }
    body.light-theme .message.sent { color: #ffffff; }
    .message-info { font-size: 0.7rem; color: var(--text-muted);
        margin-top: 4px; text-align: right; }
    .message.sent .message-info i { color: var(--neon-green); }
    .chat-input-container { display: flex; gap: 10px; margin-bottom: 20px; }
    .chat-input-container input { flex: 1; background-color: var(--bg-input);
        border: 1px solid var(--border-color); border-radius: 8px;
        padding: 12px 15px; color: var(--text-main); outline: none; }
    .chat-input-container button { background-color: var(--accent-green);
        color: white; border: none; border-radius: 8px; padding: 0 15px;
        cursor: pointer; display: flex; align-items: center; gap: 8px;
        transition: var(--transition); }
    .chat-input-container button:hover { background-color: var(--neon-green); color: black; }
    .quick-messages h3 { font-size: 0.9rem; color: var(--text-muted); margin-bottom: 10px; }
    .quick-btns { display: flex; flex-wrap: wrap; gap: 8px; }
    .q-btn { background: none; border: 1px solid var(--accent-green);
        color: var(--neon-green); padding: 8px 15px; border-radius: 6px;
        font-size: 0.8rem; cursor: pointer; transition: var(--transition); }
    .q-btn:hover { background-color: var(--accent-green); color: white; }
    .q-btn.alert { border-color: var(--danger); color: var(--danger); }
    .q-btn.alert:hover { background-color: var(--danger); color: white; }
    .grid-container { display: grid; grid-template-columns: 1fr 1fr;
        gap: 15px; margin-bottom: 20px; }
    .status-card { background-color: var(--bg-card);
        border: 1px solid var(--border-color); border-radius: 12px; padding: 15px; }
    .status-card label { display: block; font-size: 0.75rem;
        color: var(--text-muted); margin-bottom: 10px; }
    .status-value { display: flex; align-items: center; gap: 12px; }
    .val-text { display: block; font-weight: 600; font-size: 1rem; }
    .status-value small { color: var(--text-muted); font-size: 0.7rem; }
    .dot { width: 12px; height: 12px; border-radius: 50%; }
    .dot.online { background-color: var(--neon-green);
        box-shadow: 0 0 8px var(--neon-green); }
    .icon-large { font-size: 1.5rem; color: var(--text-muted); }
    .active-neon { color: var(--neon-green) !important; }
    .signal-bars { display: flex; align-items: flex-end; gap: 2px; height: 20px; }
    .bar { width: 4px; background-color: #333; border-radius: 1px; }
    body.light-theme .bar { background-color: #d3d8de; }
    .bar:nth-child(1) { height: 20%; } .bar:nth-child(2) { height: 40%; }
    .bar:nth-child(3) { height: 60%; } .bar:nth-child(4) { height: 80%; }
    .bar:nth-child(5) { height: 100%; }
    .bar.active { background-color: var(--neon-green); }
    .battery-section { background-color: var(--bg-card);
        border: 1px solid var(--border-color); border-radius: 12px; padding: 15px; }
    .battery-section label { font-size: 0.8rem; color: var(--text-muted);
        margin-bottom: 10px; display: block; }
    .battery-container { display: flex; align-items: center; gap: 15px; }
    .battery-container i { color: var(--neon-green); font-size: 1.2rem; }
    .battery-bar-bg { flex: 1; height: 8px; background-color: #232d38;
        border-radius: 4px; overflow: hidden; }
    body.light-theme .battery-bar-bg { background-color: #e2e6eb; }
    .battery-bar-fill { height: 100%; background-color: var(--neon-green);
        box-shadow: 0 0 10px var(--neon-green); }
    .battery-pct { font-weight: 600; font-size: 0.9rem; }
    .header-actions { display: flex; gap: 10px; }
    .search-box { position: relative; flex: 1; }
    .search-box input { width: 100%; background-color: var(--bg-input);
        border: 1px solid var(--border-color); border-radius: 6px;
        padding: 8px 10px 8px 30px; color: var(--text-main); font-size: 0.8rem; }
    .search-box i { position: absolute; left: 10px; top: 50%;
        transform: translateY(-50%); color: var(--text-muted); font-size: 0.8rem; }
    .btn-outline-danger { background: none; border: 1px solid var(--danger);
        color: var(--danger); padding: 5px 10px; border-radius: 6px;
        font-size: 0.8rem; cursor: pointer; }
    .history-list { display: flex; flex-direction: column;
        gap: 10px; margin-bottom: 20px; }
    .history-item { background-color: var(--bg-card);
        border: 1px solid var(--border-color); padding: 12px 15px;
        border-radius: 8px; display: flex; justify-content: space-between;
        align-items: center; }
    .hist-text { font-size: 0.85rem; color: var(--text-main); }
    .hist-meta { text-align: right; display: flex; flex-direction: column; gap: 4px; }
    .hist-meta span:first-child { font-size: 0.7rem; color: var(--text-muted); }
    .type-in { color: var(--neon-green); font-size: 0.7rem; font-weight: 600; }
    .type-out { color: var(--text-muted); font-size: 0.7rem; font-weight: 600; }
    .pagination { display: flex; justify-content: center; gap: 5px; }
    .pagination button { background-color: var(--bg-card);
        border: 1px solid var(--border-color); color: var(--text-muted);
        width: 32px; height: 32px; border-radius: 4px;
        cursor: pointer; font-size: 0.8rem; }
    .pagination button.active { background-color: var(--neon-green);
        color: black; border-color: var(--neon-green); }
    .settings-list { display: flex; flex-direction: column; gap: 15px; }
    .setting-item { display: flex; justify-content: space-between;
        align-items: center; padding: 12px 0;
        border-bottom: 1px solid var(--border-color); }
    .setting-label { display: flex; align-items: center; gap: 12px; font-size: 0.9rem; }
    .setting-label i { color: var(--text-muted); width: 20px; }
    .setting-item input[type="text"], .setting-item select {
        background-color: var(--bg-input); border: 1px solid var(--border-color);
        color: var(--text-main); padding: 8px 12px; border-radius: 6px;
        outline: none; font-size: 0.85rem; }
    .switch { position: relative; display: inline-block; width: 44px; height: 22px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0;
        right: 0; bottom: 0; background-color: #333;
        transition: .4s; border-radius: 22px; }
    body.light-theme .slider { background-color: #ccd2d8; }
    .slider:before { position: absolute; content: ""; height: 16px; width: 16px;
        left: 3px; bottom: 3px; background-color: white;
        transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--neon-green); }
    input:checked + .slider:before { transform: translateX(22px); }
    .btn-danger { background: none; border: 1px solid var(--danger);
        color: var(--danger); padding: 8px 15px; border-radius: 6px;
        cursor: pointer; transition: var(--transition); }
    .btn-danger:hover { background-color: var(--danger); color: white; }
    .btn-neon { background: none; border: 1px solid var(--neon-green);
        color: var(--neon-green); padding: 8px 15px; border-radius: 6px;
        cursor: pointer; transition: var(--transition); }
    .btn-neon:hover { background-color: var(--neon-green); color: black; }
    .main-footer { padding: 15px 20px; border-top: 1px solid var(--border-color);
        display: flex; justify-content: space-between;
        font-size: 0.7rem; color: var(--text-muted); }
    .footer-left i { color: var(--neon-green); margin-right: 5px; }
    ::-webkit-scrollbar { width: 5px; }
    ::-webkit-scrollbar-track { background: var(--bg-dark); }
    ::-webkit-scrollbar-thumb { background: var(--border-color); border-radius: 10px; }
    ::-webkit-scrollbar-thumb:hover { background: var(--text-muted); }
    .empty-state { text-align: center; color: var(--text-muted);
        font-size: 0.9rem; padding: 2rem 1rem; opacity: 0.7;
        pointer-events: none; user-select: none; }
    </style>
</head>
<body class="light-theme">
<div class="app-container">
    <header class="main-header">
        <div class="logo-container">
            <i class="fas fa-wave-square logo-icon"></i>
            <h1 class="logo-text">REC-WAVE</h1>
        </div>
        <div class="header-status">
            <span id="device-name-header">REC-WAVE 01</span>
        </div>
    </header>
    <nav class="tab-navigation">
        <button class="tab-btn active" data-tab="chat">
            <i class="far fa-comment-dots"></i> <span data-i18n="tabChat">Chat</span>
        </button>
        <button class="tab-btn" data-tab="system">
            <i class="fas fa-chart-line"></i> <span data-i18n="tabStatus">Status</span>
        </button>
        <button class="tab-btn" data-tab="diagnostics">
            <i class="far fa-clipboard"></i> <span data-i18n="tabHistory">Histórico</span>
        </button>
        <button class="tab-btn" data-tab="settings">
            <i class="fas fa-cog"></i> <span data-i18n="tabSettings">Config</span>
        </button>
        <button class="tab-btn" data-tab="extras">
            <i class="fas fa-th-large"></i> <span data-i18n="tabExtras">Extras</span>
        </button>
    </nav>
    <main class="content-area">
        <section id="chat" class="tab-content active">
            <div class="section-header">
                <h2 data-i18n="chatTitle">Conversa</h2>
                <span class="status-indicator online" data-i18n="connected">Conectado</span>
            </div>
            <div class="chat-window" id="chat-messages"></div>
            <div class="chat-input-container">
                <input type="text" id="message-input" data-i18n-placeholder="msgPlaceholder" placeholder="Digite sua mensagem...">
                <button id="send-btn"><i class="fas fa-paper-plane"></i> <span data-i18n="send">Enviar</span></button>
            </div>
            <div class="quick-messages">
                <h3 data-i18n="quickMsgs">Mensagens rápidas</h3>
                <div class="quick-btns">
                    <button class="q-btn">OK</button>
                    <button class="q-btn">Subindo</button>
                    <button class="q-btn">Descendo</button>
                    <button class="q-btn">Ajuda</button>
                    <button class="q-btn alert">SOS</button>
                </div>
            </div>
        </section>
        <section id="system" class="tab-content">
            <div class="section-header">
                <h2 data-i18n="systemStatus">Status do Sistema</h2>
            </div>
            <div class="grid-container">
                <div class="status-card">
                    <label data-i18n="connection">Conexão</label>
                    <div class="status-value">
                        <span class="dot"></span>
                        <div><span class="val-text">--</span><small>--</small></div>
                    </div>
                </div>
                <div class="status-card">
                    <label data-i18n="signalQuality">Qualidade do sinal</label>
                    <div class="status-value">
                        <div class="signal-bars">
                            <div class="bar"></div><div class="bar"></div>
                            <div class="bar"></div><div class="bar"></div>
                            <div class="bar"></div>
                        </div>
                        <div><span class="val-text">--</span><small>--</small></div>
                    </div>
                </div>
                <div class="status-card">
                    <label data-i18n="latency">Latência</label>
                    <div class="status-value">
                        <i class="far fa-clock icon-large"></i>
                        <div><span class="val-text">--</span><small data-i18n="responseTime">Tempo de resposta</small></div>
                    </div>
                </div>
                <div class="status-card">
                    <label data-i18n="commStatus">Status da comunicação</label>
                    <div class="status-value">
                        <i class="fas fa-wave-square icon-large"></i>
                        <div><span class="val-text">--</span><small>--</small></div>
                    </div>
                </div>
                <div class="status-card">
                    <label data-i18n="msgSent">Mensagens enviadas</label>
                    <div class="status-value">
                        <i class="far fa-paper-plane icon-large"></i>
                        <div><span class="val-text">--</span><small data-i18n="total">Total</small></div>
                    </div>
                </div>
                <div class="status-card">
                    <label data-i18n="msgReceived">Mensagens recebidas</label>
                    <div class="status-value">
                        <i class="fas fa-download icon-large"></i>
                        <div><span class="val-text">--</span><small data-i18n="total">Total</small></div>
                    </div>
                </div>
            </div>
            <div class="battery-section">
                <label data-i18n="battery">Bateria (ESP32)</label>
                <div class="battery-container">
                    <i class="fas fa-battery-three-quarters"></i>
                    <div class="battery-bar-bg">
                        <div class="battery-bar-fill" style="width: 0%;"></div>
                    </div>
                    <span class="battery-pct">--</span>
                </div>
            </div>
        </section>
        <section id="diagnostics" class="tab-content">
            <div class="section-header">
                <h2 data-i18n="msgHistory">Histórico de Mensagens</h2>
                <div class="header-actions">
                    <div class="search-box">
                        <input type="text" data-i18n-placeholder="searchMsg" placeholder="Buscar mensagem...">
                        <i class="fas fa-search"></i>
                    </div>
                    <button class="btn-outline-danger">
                        <i class="far fa-trash-alt"></i> <span data-i18n="clear">Limpar</span>
                    </button>
                </div>
            </div>
            <div class="history-list"></div>
            <div class="pagination">
                <button data-p="1"><i class="fas fa-angle-double-left"></i></button>
                <button data-p="1"><i class="fas fa-angle-left"></i></button>
                <button class="active" data-p="1">1</button>
                <button data-p="1"><i class="fas fa-angle-right"></i></button>
                <button data-p="1"><i class="fas fa-angle-double-right"></i></button>
            </div>
        </section>
        <section id="settings" class="tab-content">
            <div class="section-header">
                <h2 data-i18n="settingsTitle">Configurações</h2>
            </div>
            <div class="settings-list">
                <div class="setting-item">
                    <div class="setting-label">
                        <i class="far fa-user"></i> <span data-i18n="deviceName">Nome do dispositivo</span>
                    </div>
                    <input type="text" value="REC-WAVE 01">
                </div>
                <div class="setting-item">
                    <div class="setting-label">
                        <i class="far fa-moon"></i> <span data-i18n="darkMode">Modo escuro</span>
                    </div>
                    <label class="switch">
                        <input type="checkbox" checked>
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="setting-item">
                    <div class="setting-label">
                        <i class="fas fa-globe"></i> <span data-i18n="language">Idioma</span>
                    </div>
                    <select>
                        <option value="Português" selected>Português</option>
                        <option value="English">English</option>
                    </select>
                </div>
                <div class="setting-item action-item">
                    <div class="setting-label">
                        <i class="fas fa-sync"></i> <span data-i18n="restartSystem">Reiniciar sistema</span>
                    </div>
                    <button class="btn-danger" data-i18n="restart">Reiniciar</button>
                </div>
            </div>
        </section>
        <section id="extras" class="tab-content">
            <div class="section-header">
                <h2 data-i18n="tabExtras">Extras</h2>
            </div>
            <div class="empty-state" data-i18n="extrasEmpty">Em breve.</div>
        </section>
    </main>
    <footer class="main-footer">
        <div class="footer-left">
            <i class="fas fa-wave-square"></i> <span data-i18n="footerTagline">REC-WAVE - Comunicação que atravessa as águas.</span>
        </div>
        <div class="footer-right">
            <span data-i18n="version">Versão</span> 1.0.0
        </div>
    </footer>
</div>
<script>
const BASE = "";

const translations = {
  'Português': {
    tabChat: 'Chat', tabStatus: 'Status', tabHistory: 'Histórico',
    tabSettings: 'Config', tabExtras: 'Extras',
    chatTitle: 'Conversa', connected: 'Conectado', disconnected: 'Desconectado',
    msgPlaceholder: 'Digite sua mensagem...', send: 'Enviar',
    quickMsgs: 'Mensagens rápidas',
    systemStatus: 'Status do Sistema', connection: 'Conexão',
    signalQuality: 'Qualidade do sinal', latency: 'Latência',
    responseTime: 'Tempo de resposta', commStatus: 'Status da comunicação',
    msgSent: 'Mensagens enviadas', msgReceived: 'Mensagens recebidas',
    total: 'Total', battery: 'Bateria (ESP32)',
    msgHistory: 'Histórico de Mensagens', searchMsg: 'Buscar mensagem...',
    clear: 'Limpar', settingsTitle: 'Configurações',
    deviceName: 'Nome do dispositivo', darkMode: 'Modo escuro',
    language: 'Idioma', restartSystem: 'Reiniciar sistema', restart: 'Reiniciar',
    extrasEmpty: 'Em breve.',
    footerTagline: 'REC-WAVE - Comunicação que atravessa as águas.', version: 'Versão',
    online: 'Online', offline: 'Offline', noConnection: 'Sem conexão',
    excellent: 'Excelente', good: 'Bom', fair: 'Regular', weak: 'Fraco',
    active: 'Ativa', inactive: 'Inativa', uptime: 'Uptime',
    waitingData: 'Aguardando dados...', received: 'Recebida', sent: 'Enviada',
    confirmClearHistory: 'Limpar todo o histórico?',
    confirmRestart: 'Reiniciar o sistema?', restarting: 'Reiniciando...'
  },
  'English': {
    tabChat: 'Chat', tabStatus: 'Status', tabHistory: 'History',
    tabSettings: 'Settings', tabExtras: 'Extras',
    chatTitle: 'Conversation', connected: 'Connected', disconnected: 'Disconnected',
    msgPlaceholder: 'Type your message...', send: 'Send',
    quickMsgs: 'Quick messages',
    systemStatus: 'System Status', connection: 'Connection',
    signalQuality: 'Signal quality', latency: 'Latency',
    responseTime: 'Response time', commStatus: 'Communication status',
    msgSent: 'Messages sent', msgReceived: 'Messages received',
    total: 'Total', battery: 'Battery (ESP32)',
    msgHistory: 'Message History', searchMsg: 'Search message...',
    clear: 'Clear', settingsTitle: 'Settings',
    deviceName: 'Device name', darkMode: 'Dark mode',
    language: 'Language', restartSystem: 'Restart system', restart: 'Restart',
    extrasEmpty: 'Coming soon.',
    footerTagline: 'REC-WAVE - Communication that crosses the waters.', version: 'Version',
    online: 'Online', offline: 'Offline', noConnection: 'No connection',
    excellent: 'Excellent', good: 'Good', fair: 'Fair', weak: 'Weak',
    active: 'Active', inactive: 'Inactive', uptime: 'Uptime',
    waitingData: 'Waiting for data...', received: 'Received', sent: 'Sent',
    confirmClearHistory: 'Clear the entire history?',
    confirmRestart: 'Restart the system?', restarting: 'Restarting...'
  }
};

let currentLang = 'Português';
function t(key) {
  return (translations[currentLang] && translations[currentLang][key]) || key;
}

function applyStaticTranslations() {
  document.querySelectorAll('[data-i18n]').forEach(el => {
    el.textContent = t(el.dataset.i18n);
  });
  document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
    el.placeholder = t(el.dataset.i18nPlaceholder);
  });
}

function applyTheme(isDark) {
  document.body.classList.toggle('light-theme', !isDark);
}

document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.tab).classList.add('active');
    if (btn.dataset.tab === 'system')      loadStatus();
    if (btn.dataset.tab === 'diagnostics') loadHistory();
    if (btn.dataset.tab === 'settings')    loadSettings();
  });
});

const chatWindow = document.getElementById('chat-messages');
const msgInput   = document.getElementById('message-input');
const sendBtn    = document.getElementById('send-btn');

async function loadChatMessages() {
  try {
    const res = await fetch(BASE + '/api/chat');
    const data = await res.json();
    chatWindow.innerHTML = '';
    data.messages.forEach(m => {
      const div = document.createElement('div');
      div.className = 'message ' + (m.type === 'out' ? 'sent' : 'received');
      div.innerHTML = escapeHTML(m.text) +
        '<div class="message-info">' + m.time +
        (m.type === 'out' ? ' <i class="fas fa-check-double"></i>' : '') +
        '</div>';
      chatWindow.appendChild(div);
    });
    chatWindow.scrollTop = chatWindow.scrollHeight;
  } catch (e) { console.log('Erro chat:', e); }
}

async function sendMessage() {
  const text = msgInput.value.trim();
  if (!text) return;
  try {
    await fetch(BASE + '/api/chat/send', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ message: text })
    });
    msgInput.value = '';
    loadChatMessages();
    loadStatus();
  } catch (e) { console.log('Erro enviar:', e); }
}

sendBtn.addEventListener('click', sendMessage);
msgInput.addEventListener('keydown', e => { if (e.key === 'Enter') sendMessage(); });

document.querySelectorAll('.q-btn').forEach(btn => {
  btn.addEventListener('click', async () => {
    const text = btn.textContent.trim();
    try {
      await fetch(BASE + '/api/chat/quick', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: text })
      });
      loadChatMessages();
      loadStatus();
      if (text === 'SOS') alert('ALERTA SOS ENVIADO!');
    } catch (e) { console.log('Erro quick:', e); }
  });
});

async function loadStatus() {
  try {
    const res = await fetch(BASE + '/api/status');
    const data = await res.json();
    const cards = document.querySelectorAll('.status-card');

    const dot = cards[0].querySelector('.dot');
    dot.className = 'dot' + (data.connected ? ' online' : '');
    cards[0].querySelector('.val-text').textContent = data.connected ? t('online') : t('offline');
    cards[0].querySelector('small').textContent = data.connected ? data.wifiMode : t('noConnection');

    const rssi = data.rssi;
    const bars = cards[1].querySelectorAll('.bar');
    const barCount = Math.min(5, Math.max(0, Math.round((rssi + 100) / 20)));
    bars.forEach((b, i) => b.classList.toggle('active', i < barCount));
    cards[1].querySelector('.val-text').textContent = rssi + ' dBm';
    cards[1].querySelector('small').textContent =
      (rssi >= -50) ? t('excellent') : (rssi >= -70) ? t('good') : (rssi >= -85) ? t('fair') : t('weak');

    cards[2].querySelector('.val-text').textContent = data.latency + ' ms';

    const commIcon = cards[3].querySelector('.icon-large');
    commIcon.className = 'fas fa-wave-square icon-large' + (data.connected ? ' active-neon' : '');
    cards[3].querySelector('.val-text').textContent = data.connected ? t('active') : t('inactive');
    cards[3].querySelector('small').textContent = data.connected ? t('uptime') + ': ' + data.uptime : '--';

    cards[4].querySelector('.val-text').textContent = data.msgSent;
    cards[5].querySelector('.val-text').textContent = data.msgReceived;

    const batPct = data.battery;
    document.querySelector('.battery-bar-fill').style.width = batPct + '%';
    document.querySelector('.battery-pct').textContent = batPct + '%';
  } catch (e) { console.log('Erro status:', e); }
}

const PER_PAGE = 5;
let historyPage = 1;

async function loadHistory(filter) {
  try {
    const res = await fetch(BASE + '/api/history');
    const data = await res.json();
    let msgs = data.messages;
    if (filter) {
      const f = filter.toLowerCase();
      msgs = msgs.filter(m => m.text.toLowerCase().includes(f));
    }
    const totalPages = Math.max(1, Math.ceil(msgs.length / PER_PAGE));
    if (historyPage > totalPages) historyPage = totalPages;
    const start = (historyPage - 1) * PER_PAGE;
    const page = msgs.slice(start, start + PER_PAGE);

    const list = document.querySelector('.history-list');
    list.innerHTML = '';
    if (page.length === 0) {
      list.innerHTML = '<div class="empty-state">' + t('waitingData') + '</div>';
    }
    page.forEach(m => {
      const div = document.createElement('div');
      div.className = 'history-item';
      div.innerHTML =
        '<span class="hist-text">' + escapeHTML(m.text) + '</span>' +
        '<div class="hist-meta">' +
          '<span>' + m.time + '</span>' +
          '<span class="' + (m.type === 'in' ? 'type-in' : 'type-out') + '">' +
          (m.type === 'in' ? t('received') : t('sent')) + '</span>' +
        '</div>';
      list.appendChild(div);
    });

    const pag = document.querySelector('.pagination');
    pag.innerHTML = '';
    pag.innerHTML += '<button data-p="1"><i class="fas fa-angle-double-left"></i></button>';
    pag.innerHTML += '<button data-p="' + Math.max(1, historyPage - 1) + '"><i class="fas fa-angle-left"></i></button>';
    for (let i = 1; i <= totalPages; i++) {
      pag.innerHTML += '<button class="' + (i === historyPage ? 'active' : '') + '" data-p="' + i + '">' + i + '</button>';
    }
    pag.innerHTML += '<button data-p="' + Math.min(totalPages, historyPage + 1) + '"><i class="fas fa-angle-right"></i></button>';
    pag.innerHTML += '<button data-p="' + totalPages + '"><i class="fas fa-angle-double-right"></i></button>';

    pag.querySelectorAll('button').forEach(b => {
      b.addEventListener('click', () => {
        historyPage = parseInt(b.dataset.p);
        loadHistory(document.querySelector('.search-box input').value);
      });
    });
  } catch (e) { console.log('Erro histórico:', e); }
}

document.querySelector('.search-box input').addEventListener('input', e => {
  historyPage = 1;
  loadHistory(e.target.value);
});

document.querySelector('.btn-outline-danger').addEventListener('click', async () => {
  if (confirm(t('confirmClearHistory'))) {
    try {
      await fetch(BASE + '/api/history/clear', { method: 'POST' });
      loadHistory();
      loadChatMessages();
      loadStatus();
    } catch (e) { console.log('Erro limpar:', e); }
  }
});

function getEl() {
  const items = document.querySelectorAll('.setting-item');
  return {
    name:   items[0].querySelector('input'),
    dark:   items[1].querySelector('input[type=checkbox]'),
    lang:   items[2].querySelector('select'),
    rstBtn: items[3].querySelector('button')
  };
}

async function loadSettings() {
  try {
    const res = await fetch(BASE + '/api/settings');
    const data = await res.json();
    const el = getEl();
    el.name.value = data.deviceName;
    el.dark.checked = data.darkMode;
    el.lang.value = data.language;
    currentLang = data.language;
    applyTheme(data.darkMode);
    applyStaticTranslations();
    document.getElementById('device-name-header').textContent = data.deviceName;
  } catch (e) { console.log('Erro settings:', e); }
}

document.addEventListener('DOMContentLoaded', () => {
  getEl().dark.addEventListener('change', e => {
    applyTheme(e.target.checked);
    saveSettings();
  });
  getEl().lang.addEventListener('change', e => {
    currentLang = e.target.value;
    applyStaticTranslations();
    const active = document.querySelector('.tab-content.active');
    if (active && active.id === 'system')      loadStatus();
    if (active && active.id === 'diagnostics') loadHistory();
    saveSettings();
  });
});

async function saveSettings() {
  const el = getEl();
  const payload = {
    deviceName: el.name.value,
    darkMode: el.dark.checked,
    language: el.lang.value
  };
  try {
    const res = await fetch(BASE + '/api/settings/update', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    document.getElementById('device-name-header').textContent = data.deviceName;
  } catch (e) { console.log('Erro salvar config:', e); }
}

document.querySelector('.tab-btn[data-tab="settings"]').addEventListener('click', () => {
  setTimeout(saveSettings, 300);
});

document.addEventListener('DOMContentLoaded', () => {
  getEl().rstBtn.addEventListener('click', async () => {
    if (confirm(t('confirmRestart'))) {
      try {
        await fetch(BASE + '/api/settings/restart', { method: 'POST' });
        alert(t('restarting'));
      } catch (e) { console.log('Erro restart:', e); }
    }
  });
});

setInterval(() => {
  const active = document.querySelector('.tab-content.active');
  if (!active) return;
  if (active.id === 'chat')   loadChatMessages();
  if (active.id === 'system') loadStatus();
}, 3000);

function escapeHTML(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

loadChatMessages();
loadSettings();
</script>
</body>
</html>
)rawliteral";

// devolve o tempo desde o boot no formato HH:MM:SS
String formatUptime() {
  unsigned long secs = millis() / 1000;         // converte millis em segundos
  unsigned int h = (secs / 3600) % 24;           // calcula as horas
  unsigned int m = (secs / 60) % 60;             // calcula os minutos
  unsigned int s = secs % 60;                    // calcula os segundos restantes
  char buf[9];                                   // buffer para montar o texto
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, s); // formata como texto
  return String(buf);                            // retorna como String
}

// guarda uma nova mensagem no histórico do chat
void addMessage(const String &text, const String &type) {
  ChatMessage m;                 // cria uma mensagem nova
  m.text = text;                 // define o texto
  m.time = formatUptime();       // marca o horário atual
  m.type = type;                 // define se é enviada ou recebida
  messages.push_back(m);         // adiciona no fim da lista
  if (messages.size() > MAX_MESSAGES) { // se passou do limite...
    messages.erase(messages.begin());   // remove a mensagem mais antiga
  }
  if (type == "out") msgSentCount++;    // conta como enviada
  else msgReceivedCount++;              // ou conta como recebida
}

// converte um grupo de 3 bits na frequência correspondente
int obterFrequencia(String bits) {
  if (bits == "000") return 1000;  // grupo 000 -> 1000 Hz
  if (bits == "001") return 1300;  // grupo 001 -> 1300 Hz
  if (bits == "010") return 1600;  // grupo 010 -> 1600 Hz
  if (bits == "011") return 1900;  // grupo 011 -> 1900 Hz
  if (bits == "100") return 2200;  // grupo 100 -> 2200 Hz
  if (bits == "101") return 2500;  // grupo 101 -> 2500 Hz
  if (bits == "110") return 2800;  // grupo 110 -> 2800 Hz
  if (bits == "111") return 3100;  // grupo 111 -> 3100 Hz
  return -1;                       // grupo inválido
}

// toca uma frequência no buzzer por um tempo determinado
void emitirBip(int frequencia, int tempo) {
  tone(BUZZER_PIN, frequencia); // liga o buzzer na frequência escolhida
  delay(tempo);                 // mantém o som tocando pelo tempo definido
  noTone(BUZZER_PIN);           // desliga o buzzer
  delay(100);                   // pequena pausa entre os bips
}

// converte o texto em binário e transmite pelo buzzer (função bloqueante)
void transmitirTexto(const String &texto) {
  String binario = "";                          // string que vai acumular os bits
  for (unsigned int i = 0; i < texto.length(); i++) { // percorre cada caractere do texto
    char c = texto[i];                          // pega o caractere atual
    for (int b = 7; b >= 0; b--) {               // percorre os 8 bits do caractere
      binario += (c & (1 << b)) ? "1" : "0";     // adiciona "1" ou "0" conforme o bit
    }
  }

  Serial.println();                 // pula uma linha no monitor serial
  Serial.print("Transmitindo: ");   // rótulo de log
  Serial.println(texto);            // mostra o texto que será transmitido
  Serial.print("Binario: ");        // rótulo de log
  Serial.println(binario);          // mostra a sequência binária gerada

  emitirBip(FREQUENCIA_CONTROLE, 500); // emite o sinal de início da transmissão

  int tamanho = binario.length();  // tamanho total da sequência de bits
  int i = 0;                       // posição atual na sequência
  while (i + 3 <= tamanho) {                 // enquanto houver grupos completos de 3 bits
    String grupo = binario.substring(i, i + 3); // extrai o grupo de 3 bits
    int freq = obterFrequencia(grupo);           // converte o grupo em frequência
    emitirBip(freq, 300);                        // emite o bip correspondente
    i += 3;                                      // avança para o próximo grupo
  }

  while (i < tamanho) {                 // trata os bits restantes que não formam um grupo de 3
    char bit = binario[i];              // pega o bit atual
    if (bit == '0') emitirBip(FREQUENCIA_BIT_0, 300); // emite frequência de bit 0
    else            emitirBip(FREQUENCIA_BIT_1, 300); // emite frequência de bit 1
    i++;                                 // avança para o próximo bit
  }

  emitirBip(FREQUENCIA_CONTROLE, 500); // emite o sinal de fim da transmissão

  Serial.println("Transmissao concluida."); // log de conclusão
}

// leitura da porcentagem de bateria (fixa até haver sensor real)
int readBatteryPercent() {
  return 82; // valor fixo, trocar por leitura de um pino ADC futuramente
}

// leitura da latência de comunicação (calculada a partir do millis por enquanto)
int simulateLatency() {
  return 20 + (millis() % 40); // gera um valor variável entre 20 e 59 ms
}

// entrega a página HTML principal ao navegador
void handleRoot() {
  server.send_P(200, "text/html", htmlPage); // envia a página guardada em PROGMEM
}

// monta o JSON com a lista de mensagens e envia como resposta
void sendMessagesJson() {
  DynamicJsonDocument doc(8192);           // documento JSON com espaço reservado
  JsonArray arr = doc.createNestedArray("messages"); // cria o array "messages"
  for (auto &m : messages) {               // percorre todas as mensagens guardadas
    JsonObject o = arr.createNestedObject(); // cria um objeto JSON para cada mensagem
    o["text"] = m.text;                      // grava o texto
    o["time"] = m.time;                      // grava o horário
    o["type"] = m.type;                      // grava o tipo (in/out)
  }
  String out;                    // string que vai receber o JSON serializado
  serializeJson(doc, out);       // converte o documento em texto JSON
  server.send(200, "application/json", out); // envia a resposta ao cliente
}

// rota GET /api/chat -> retorna as mensagens do chat
void handleGetChat() {
  sendMessagesJson(); // reaproveita a função que monta o JSON
}

// rota GET /api/history -> retorna o histórico completo
void handleGetHistory() {
  sendMessagesJson(); // reaproveita a mesma lista de mensagens
}

// rota POST /api/chat/send -> recebe e processa uma nova mensagem enviada
void handlePostChatSend() {
  if (!server.hasArg("plain")) {           // verifica se veio corpo na requisição
    server.send(400, "application/json", "{\"error\":\"corpo vazio\"}"); // erro se não veio
    return;                                // interrompe a função
  }
  StaticJsonDocument<256> doc;             // documento para interpretar o JSON recebido
  DeserializationError err = deserializeJson(doc, server.arg("plain")); // tenta ler o JSON
  if (err) {                               // se houve erro ao interpretar
    server.send(400, "application/json", "{\"error\":\"json invalido\"}"); // responde com erro
    return;                                // interrompe a função
  }
  String text = doc["message"] | "";       // extrai o campo "message" (vazio se não existir)
  if (text.length() == 0) {                // verifica se a mensagem está vazia
    server.send(400, "application/json", "{\"error\":\"mensagem vazia\"}"); // responde com erro
    return;                                // interrompe a função
  }
  addMessage(text, "out");                 // salva a mensagem como enviada
  server.send(200, "application/json", "{\"ok\":true}"); // confirma o recebimento ao navegador

  transmitirTexto(text); // transmite o texto pelo buzzer (função bloqueante)
}

// rota POST /api/chat/quick -> mensagens rápidas usam a mesma lógica do envio normal
void handlePostChatQuick() {
  handlePostChatSend(); // reaproveita a função de envio de mensagem
}

// rota POST /api/history/clear -> apaga todo o histórico de mensagens
void handlePostHistoryClear() {
  messages.clear();          // esvazia a lista de mensagens
  msgSentCount = 0;          // zera o contador de enviadas
  msgReceivedCount = 0;      // zera o contador de recebidas
  server.send(200, "application/json", "{\"ok\":true}"); // confirma a limpeza
}

// rota GET /api/status -> retorna o status atual do sistema
void handleGetStatus() {
  bool connected = (WiFi.status() == WL_CONNECTED); // verifica se o WiFi está conectado
  DynamicJsonDocument doc(512);      // documento JSON para montar a resposta
  doc["connected"]    = connected;              // informa se está conectado
  doc["wifiMode"]     = "STA";                  // modo do WiFi (estação)
  doc["rssi"]         = connected ? WiFi.RSSI() : -100; // força do sinal
  doc["latency"]      = simulateLatency();      // latência atual
  doc["uptime"]       = formatUptime();         // tempo ligado
  doc["msgSent"]      = msgSentCount;           // total de mensagens enviadas
  doc["msgReceived"]  = msgReceivedCount;       // total de mensagens recebidas
  doc["battery"]      = readBatteryPercent();   // porcentagem da bateria
  String out;                        // string para o JSON final
  serializeJson(doc, out);           // serializa o documento em texto
  server.send(200, "application/json", out); // envia a resposta
}

// rota GET /api/settings -> retorna as configurações atuais
void handleGetSettings() {
  DynamicJsonDocument doc(512);   // documento JSON para as configurações
  doc["deviceName"] = cfgDeviceName; // nome do dispositivo
  doc["darkMode"]   = cfgDarkMode;   // estado do modo escuro
  doc["language"]   = cfgLanguage;   // idioma atual
  String out;                     // string para o JSON final
  serializeJson(doc, out);        // serializa o documento
  server.send(200, "application/json", out); // envia a resposta
}

// rota POST /api/settings/update -> atualiza as configurações do dispositivo
void handlePostSettingsUpdate() {
  if (!server.hasArg("plain")) {      // verifica se a requisição tem corpo
    server.send(400, "application/json", "{\"error\":\"corpo vazio\"}"); // erro se não tiver
    return;                           // interrompe a função
  }
  StaticJsonDocument<512> doc;        // documento para interpretar o JSON recebido
  DeserializationError err = deserializeJson(doc, server.arg("plain")); // tenta ler o JSON
  if (err) {                          // se houve erro na leitura
    server.send(400, "application/json", "{\"error\":\"json invalido\"}"); // responde com erro
    return;                           // interrompe a função
  }
  if (doc.containsKey("deviceName")) cfgDeviceName = doc["deviceName"].as<String>(); // atualiza nome
  if (doc.containsKey("darkMode"))   cfgDarkMode   = doc["darkMode"].as<bool>();     // atualiza tema
  if (doc.containsKey("language"))   cfgLanguage   = doc["language"].as<String>();   // atualiza idioma

  handleGetSettings(); // responde já com as configurações atualizadas
}

// rota POST /api/settings/restart -> agenda o reinício da ESP32
void handlePostSettingsRestart() {
  server.send(200, "application/json", "{\"ok\":true,\"message\":\"Reiniciando...\"}"); // confirma antes de reiniciar
  restartPending = true;         // sinaliza que o reinício deve acontecer
  restartAt = millis() + 500;    // agenda o reinício meio segundo à frente
}

// rota usada quando nenhuma outra rota corresponde ao pedido
void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"rota nao encontrada\"}"); // responde com erro 404
}

// configuração inicial, executada uma única vez ao ligar a ESP32
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // desativa o detector de brownout

  Serial.begin(115200); // inicia a comunicação serial em 115200 baud
  delay(1000);           // aguarda a serial estabilizar

  pinMode(BUZZER_PIN, OUTPUT); // configura o pino do buzzer como saída

  Serial.println();                    // pula uma linha no log
  Serial.print("Conectando na rede: "); // mensagem de log
  Serial.println(ssid);                 // mostra o nome da rede

  WiFi.mode(WIFI_STA);                    // define o ESP32 como estação WiFi (cliente)
  WiFi.setTxPower(WIFI_POWER_11dBm);      // reduz a potência de transmissão para evitar picos de corrente
  WiFi.begin(ssid, password);             // inicia a conexão com a rede

  int tentativas = 0;                                    // contador de tentativas de conexão
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) { // espera até conectar ou atingir o limite
    delay(500);          // aguarda meio segundo entre tentativas
    Serial.print(".");   // mostra progresso no log
    tentativas++;        // incrementa o contador
  }

  if (WiFi.status() == WL_CONNECTED) {      // se a conexão foi bem-sucedida
    Serial.println();                        // pula linha
    Serial.println("WiFi conectado com sucesso!"); // log de sucesso
    Serial.print("Acesse o painel em: http://");    // instrução de acesso
    Serial.println(WiFi.localIP());          // mostra o IP atribuído
  } else {                                   // se não conseguiu conectar
    Serial.println();                        // pula linha
    Serial.println("Falha ao conectar no WiFi. Verifique SSID/senha."); // log de falha
  }

  addMessage("Sistema iniciado. Pronto para comunicacao.", "in"); // mensagem inicial no chat

  server.on("/", HTTP_GET, handleRoot);                          // rota da página principal
  server.on("/api/chat", HTTP_GET, handleGetChat);                // rota de leitura do chat
  server.on("/api/chat/send", HTTP_POST, handlePostChatSend);     // rota de envio de mensagem
  server.on("/api/chat/quick", HTTP_POST, handlePostChatQuick);   // rota de mensagem rápida
  server.on("/api/status", HTTP_GET, handleGetStatus);            // rota de status do sistema
  server.on("/api/history", HTTP_GET, handleGetHistory);          // rota de histórico
  server.on("/api/history/clear", HTTP_POST, handlePostHistoryClear); // rota de limpar histórico
  server.on("/api/settings", HTTP_GET, handleGetSettings);        // rota de leitura das configurações
  server.on("/api/settings/update", HTTP_POST, handlePostSettingsUpdate); // rota de salvar configurações
  server.on("/api/settings/restart", HTTP_POST, handlePostSettingsRestart); // rota de reiniciar
  server.onNotFound(handleNotFound);                              // rota padrão para caminhos inexistentes

  server.begin();                          // inicia o servidor HTTP
  Serial.println("Servidor HTTP iniciado."); // log de confirmação
}

// laço principal, executado continuamente após o setup
void loop() {
  server.handleClient(); // processa as requisições HTTP recebidas

  if (restartPending && millis() >= restartAt) { // verifica se já é hora de reiniciar
    ESP.restart();                                // reinicia a ESP32
  }
}