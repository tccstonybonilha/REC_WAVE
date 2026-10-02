
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <arduinoFFT.h>
#include <math.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <vector>

// CONFIGURACOES DA REDE
const char* ssid     = "WorldServidor";
const char* password = "eteclab31";

// SERVIDOR WEB NA PORTA 80
WebServer server(80);

// CONFIGURACAO DO RECEPTOR DE AUDIO
// MAX9814: OUT conectado ao GPIO 32 da ESP32
#define MIC_PIN 32
const uint16_t SAMPLES = 512;
const double SAMPLING_FREQUENCY = 10000.0;
const double SIGNAL_MAGNITUDE_THRESHOLD = 50.0;
const double FREQUENCY_TOLERANCE = 140.0;

double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

// Estado do decodificador REC-WAVE
bool receivingFrame = false;
String receivedBits = "";
int activeTone = -1;
int candidateTone = -1;
uint8_t candidateCount = 0;

// ESTRUTURA DE MENSAGEM DO CHAT
struct ChatMessage {
  String text;
  String time;
  String type; // "in" (recebida) ou "out" (enviada)
};

std::vector<ChatMessage> messages;
unsigned int msgSentCount = 0;
unsigned int msgReceivedCount = 0;
const size_t MAX_MESSAGES = 100; // limite para nao estourar a memoria

// CONFIGURACOES DO DISPOSITIVO (aba "Config")
bool   cfgDarkMode   = false;
String cfgLanguage   = "Português";

// Flag para reiniciar apos responder a requisicao HTTP
bool restartPending = false;
unsigned long restartAt = 0;

// PAGINA HTML/CSS/JS EMBUTIDA (painel REC-WAVE completo)
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
    
    .icon-large { font-size: 1.5rem; color: var(--text-muted); }
    .active-neon { color: var(--neon-green) !important; }
    
    body.light-theme 
     
    body.light-theme 
    
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
            <div class="chat-input-container" style="display:none">
                <input type="text" id="message-input" data-i18n-placeholder="msgPlaceholder" placeholder="Digite sua mensagem...">
                <button id="send-btn"><i class="fas fa-paper-plane"></i> <span data-i18n="send">Enviar</span></button>
            </div>
            <div class="quick-messages" style="display:none">
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

// TRADUCOES (PT / EN)
const translations = {
    'Português': {
    tabChat: 'Chat', tabStatus: 'Status', tabHistory: 'Histórico',
    tabSettings: 'Config', tabExtras: 'Extras',
    chatTitle: 'Conversa', connected: 'Conectado', disconnected: 'Desconectado',
    msgPlaceholder: 'Digite sua mensagem...', send: 'Enviar',
    quickMsgs: 'Mensagens rápidas',
    systemStatus: 'Status do Sistema', msgSent: 'Mensagens enviadas', msgReceived: 'Mensagens recebidas',
    total: 'Total', msgHistory: 'Histórico de Mensagens', searchMsg: 'Buscar mensagem...',
    clear: 'Limpar', settingsTitle: 'Configurações',
    darkMode: 'Modo escuro',
    language: 'Idioma', restartSystem: 'Reiniciar sistema', restart: 'Reiniciar',
    extrasEmpty: 'Em breve.',
    footerTagline: 'REC-WAVE - Comunicação que atravessa as águas.', version: 'Versão',
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
    systemStatus: 'System Status', msgSent: 'Messages sent', msgReceived: 'Messages received',
    total: 'Total', msgHistory: 'Message History', searchMsg: 'Search message...',
    clear: 'Clear', settingsTitle: 'Settings',
    darkMode: 'Dark mode',
    language: 'Language', restartSystem: 'Restart system', restart: 'Restart',
    extrasEmpty: 'Coming soon.',
    footerTagline: 'REC-WAVE - Communication that crosses the waters.', version: 'Version',
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
    cards[0].querySelector('.val-text').textContent = data.msgSent;
    cards[1].querySelector('.val-text').textContent = data.msgReceived;
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
  
    dark:   items[0].querySelector('input[type=checkbox]'),
    lang:   items[1].querySelector('select'),
    rstBtn: items[2].querySelector('button')
  };
}

async function loadSettings() {
  try {
    const res = await fetch(BASE + '/api/settings');
    const data = await res.json();
    const el = getEl();
  
    el.dark.checked = data.darkMode;
    el.lang.value = data.language;
    currentLang = data.language;
    applyTheme(data.darkMode);
    applyStaticTranslations();
  
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

// FUNCOES AUXILIARES

// Formata o tempo decorrido desde o boot como HH:MM:SS
String formatUptime() {
  unsigned long secs = millis() / 1000;
  unsigned int h = (secs / 3600) % 24;
  unsigned int m = (secs / 60) % 60;
  unsigned int s = secs % 60;
  char buf[9];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, s);
  return String(buf);
}

// Adiciona uma mensagem ao historico do chat
void addMessage(const String &text, const String &type) {
  ChatMessage m;
  m.text = text;
  m.time = formatUptime();
  m.type = type;
  messages.push_back(m);
  if (messages.size() > MAX_MESSAGES) {
    messages.erase(messages.begin());
  }
  if (type == "out") msgSentCount++;
  else msgReceivedCount++;
}

// RECEPTOR REC-WAVE: ADC -> FFT -> frequencia -> bits -> texto
// O transmissor envia: 4000 Hz (inicio/fim), grupos de 3 bits
// em 1000..3100 Hz e os bits restantes em 3400/3700 Hz.
int classificarFrequencia(double frequencia) {
  const int frequenciasGrupos[] = {1000, 1300, 1600, 1900,
                                   2200, 2500, 2800, 3100};
  for (int i = 0; i < 8; i++) {
    if (fabs(frequencia - frequenciasGrupos[i]) <= FREQUENCY_TOLERANCE) {
      return frequenciasGrupos[i];
    }
  }
  if (fabs(frequencia - 3400.0) <= FREQUENCY_TOLERANCE) return 3400;
  if (fabs(frequencia - 3700.0) <= FREQUENCY_TOLERANCE) return 3700;
  if (fabs(frequencia - 4000.0) <= FREQUENCY_TOLERANCE) return 4000;
  return -1;
}

void finalizarQuadroRecebido() {
  if (!receivingFrame) return;

  String texto = "";
  for (unsigned int i = 0; i + 8 <= receivedBits.length(); i += 8) {
    uint8_t valor = 0;
    for (int bit = 0; bit < 8; bit++) {
      valor = (valor << 1) | (receivedBits[i + bit] == '1' ? 1 : 0);
    }
    // Mantem ASCII e bytes UTF-8, descartando apenas controle/NUL.
    if (valor >= 32 && valor != 127) texto += (char)valor;
  }

  if (texto.length() > 0) {
    addMessage(texto, "in");
    Serial.print("Mensagem recebida: ");
    Serial.println(texto);
  } else {
    Serial.println("Quadro recebido, mas sem texto valido.");
  }

  receivedBits = "";
  receivingFrame = false;
}

void processarTom(int tom) {
  if (tom == 4000) {
    if (receivingFrame) finalizarQuadroRecebido();
    receivingFrame = true;
    receivedBits = "";
    Serial.println("Inicio de quadro REC-WAVE.");
    return;
  }

  if (!receivingFrame) return;

  if (tom == 3400) {
    receivedBits += '0';
  } else if (tom == 3700) {
    receivedBits += '1';
  } else {
    const char* grupos[] = {"000", "001", "010", "011",
                            "100", "101", "110", "111"};
    const int frequencias[] = {1000, 1300, 1600, 1900,
                               2200, 2500, 2800, 3100};
    for (int i = 0; i < 8; i++) {
      if (tom == frequencias[i]) {
        receivedBits += grupos[i];
        break;
      }
    }
  }
}

void processarBlocoDeAudio() {
  const unsigned long periodo = 1000000.0 / SAMPLING_FREQUENCY;

  for (uint16_t i = 0; i < SAMPLES; i++) {
    unsigned long inicio = micros();
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0;
    while (micros() - inicio < periodo) {
      // sincroniza a taxa de amostragem
    }
  }

  FFT.dcRemoval();
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  double magnitudeMaxima = 0;
  for (uint16_t i = 2; i < SAMPLES / 2; i++) {
    if (vReal[i] > magnitudeMaxima) magnitudeMaxima = vReal[i];
  }

  int tom = -1;
  if (magnitudeMaxima >= SIGNAL_MAGNITUDE_THRESHOLD) {
    tom = classificarFrequencia(FFT.majorPeak());
  }

  // Exige duas leituras consecutivas iguais para evitar falsos disparos.
  if (tom < 0) {
    candidateTone = -1;
    candidateCount = 0;
    activeTone = -1;
    return;
  }

  if (tom == candidateTone) {
    if (candidateCount < 255) candidateCount++;
  } else {
    candidateTone = tom;
    candidateCount = 1;
  }

  if (candidateCount >= 2 && activeTone != candidateTone) {
    activeTone = candidateTone;
    processarTom(activeTone);
  }
}

// HANDLERS DE ROTAS

void handleRoot() {
  server.send_P(200, "text/html", htmlPage);
}

// Serializa a lista de mensagens em JSON e envia
void sendMessagesJson() {
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("messages");
  for (auto &m : messages) {
    JsonObject o = arr.createNestedObject();
    o["text"] = m.text;
    o["time"] = m.time;
    o["type"] = m.type;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleGetChat() {
  sendMessagesJson();
}

void handleGetHistory() {
  sendMessagesJson();
}

// Rota mantida para compatibilidade, mas este dispositivo nao transmite
void handlePostChatSend() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"corpo vazio\"}");
    return;
  }
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"json invalido\"}");
    return;
  }
  String text = doc["message"] | "";
  if (text.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"mensagem vazia\"}");
    return;
  }
  // Este dispositivo opera somente como receptor.
  server.send(405, "application/json", "{\"error\":\"dispositivo configurado somente como receptor\"}");

}

// Mesma logica do envio normal, usada pelos botoes de mensagem rapida
void handlePostChatQuick() {
  handlePostChatSend();
}

void handlePostHistoryClear() {
  messages.clear();
  msgSentCount = 0;
  msgReceivedCount = 0;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleGetStatus() {

  DynamicJsonDocument doc(512);
    doc["msgSent"]      = msgSentCount;
  doc["msgReceived"]  = msgReceivedCount;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleGetSettings() {
  DynamicJsonDocument doc(512);

  doc["darkMode"]   = cfgDarkMode;
  doc["language"]   = cfgLanguage;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handlePostSettingsUpdate() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"corpo vazio\"}");
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"json invalido\"}");
    return;
  }

  if (doc.containsKey("darkMode"))   cfgDarkMode   = doc["darkMode"].as<bool>();
  if (doc.containsKey("language"))   cfgLanguage   = doc["language"].as<String>();

  handleGetSettings(); // responde com as configuracoes atualizadas
}

void handlePostSettingsRestart() {
  server.send(200, "application/json", "{\"ok\":true,\"message\":\"Reiniciando...\"}");
  restartPending = true;
  restartAt = millis() + 500; // da tempo da resposta HTTP ser enviada
}

void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"rota nao encontrada\"}");
}

// SETUP
void setup() {
  // Desativa o detector de brownout (evita resets falsos por
  // picos de corrente do radio WiFi em fontes/cabos fracos)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(1000);
  analogReadResolution(12);

  Serial.println();
  Serial.print("Conectando na rede: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm); // reduz pico de corrente na conexao
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi conectado com sucesso!");
    Serial.print("Acesse o painel em: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Falha ao conectar no WiFi. Verifique SSID/senha.");
  }

  // Mensagem inicial de boas-vindas no chat
  addMessage("Sistema iniciado. Pronto para comunicacao.", "in");

  // Registra as rotas
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/chat", HTTP_GET, handleGetChat);
  server.on("/api/chat/send", HTTP_POST, handlePostChatSend);
  server.on("/api/chat/quick", HTTP_POST, handlePostChatQuick);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/history", HTTP_GET, handleGetHistory);
  server.on("/api/history/clear", HTTP_POST, handlePostHistoryClear);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings/update", HTTP_POST, handlePostSettingsUpdate);
  server.on("/api/settings/restart", HTTP_POST, handlePostSettingsRestart);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

// LOOP
void loop() {
  server.handleClient();
  processarBlocoDeAudio();

  if (restartPending && millis() >= restartAt) {
    ESP.restart();
  }
}
