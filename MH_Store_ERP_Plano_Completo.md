# MH Store ERP — Plano Completo do Sistema

## 1. Visão Geral

**Nome provisório:** MH Store ERP  
**Empresa:** MHSoftware  
**Tipo de produto:** ERP Desktop Offline-First para lojistas  
**Plataformas:** Windows e Linux  
**Stack principal:** C++ + Qt 6 + QML + SQLite  
**Backend online:** FastAPI + PostgreSQL no Railway  
**Modelo comercial:** licença local + módulos e serviços online opcionais

O MH Store ERP será um sistema de gestão comercial voltado para pequenos e médios lojistas que precisam controlar vendas, estoque, clientes, fornecedores, caixa, financeiro e relatórios sem depender constantemente da internet.

A proposta central será:

> Um ERP rápido, bonito, profissional, simples de usar e que continue funcionando mesmo sem internet.

O sistema deverá unir a performance de uma aplicação nativa em C++ com uma interface moderna construída em Qt/QML e uma arquitetura offline-first.

A internet será utilizada apenas para funcionalidades que realmente necessitam dela, como:

- ativação e validação de licença;
- atualização do sistema;
- ativação de novos módulos;
- backup em nuvem;
- sincronização futura entre computadores;
- painel web futuro;
- serviços premium;
- integrações futuras.

---

# 2. Objetivo do Produto

Criar um ERP desktop moderno para lojistas em geral que substitua controles manuais, planilhas e sistemas antigos ou excessivamente complexos.

O sistema deverá atender negócios como:

- lojas de roupas;
- lojas de calçados;
- papelarias;
- lojas de ferramentas;
- autopeças;
- lojas de eletrônicos;
- lojas de presentes;
- lojas de materiais diversos;
- pequenos mercados;
- lojas de bairro;
- comércios especializados;
- pequenos distribuidores.

O foco inicial não será atender grandes redes ou operações fiscais extremamente complexas.

O foco será:

**simplicidade + velocidade + confiabilidade + experiência de uso.**

---

# 3. Proposta de Valor

## 3.1 Principais diferenciais

O MH Store deverá ser vendido com a seguinte proposta:

- funciona offline;
- não depende de servidor para realizar vendas;
- interface moderna;
- muito rápido;
- banco local;
- licença permanente disponível;
- backup local;
- backup em nuvem opcional;
- módulos adicionais opcionais;
- atualização simplificada;
- funciona em Windows e Linux;
- dados continuam pertencendo ao cliente;
- instalação simples;
- curva de aprendizado pequena.

---

# 4. Arquitetura Geral

```text
┌────────────────────────────────────┐
│           MH STORE ERP             │
│                                    │
│       C++20 + Qt 6 + QML           │
│                                    │
│ ┌────────────────────────────────┐ │
│ │       Banco Local SQLite       │ │
│ └────────────────────────────────┘ │
│                                    │
│  Vendas / Estoque / Clientes       │
│  Financeiro / Relatórios / Caixa   │
└─────────────────┬──────────────────┘
                  │
                  │ HTTPS
                  │ somente quando necessário
                  ▼
┌────────────────────────────────────┐
│        MHSoftware Cloud API        │
│                                    │
│ FastAPI + PostgreSQL + Railway     │
│                                    │
│ Licenças                           │
│ Clientes                           │
│ Dispositivos                       │
│ Módulos                            │
│ Planos                             │
│ Atualizações                       │
│ Backups                            │
└────────────────────────────────────┘
```

---

# 5. Filosofia Offline-First

O ERP deverá funcionar completamente sem internet para as funções essenciais.

## Devem funcionar offline

- cadastro de produtos;
- cadastro de clientes;
- cadastro de fornecedores;
- vendas;
- PDV;
- movimentação de estoque;
- controle de caixa;
- contas a pagar;
- contas a receber;
- relatórios locais;
- consultas;
- cadastro de usuários;
- backup local;
- impressão;
- emissão de relatórios;
- inventário.

## Dependem de internet apenas quando necessário

- validação periódica de licença;
- compra ou ativação de módulos;
- atualização automática;
- backup cloud;
- sincronização entre dispositivos;
- painel remoto;
- integrações externas;
- serviços futuros.

A perda de conexão nunca deve impedir o cliente de realizar uma venda.

---

# 6. Stack Tecnológica

## Desktop

- C++20 ou superior
- Qt 6
- Qt Quick / QML
- CMake
- SQLite
- Qt SQL
- Qt Network
- Qt PDF ou biblioteca equivalente
- Qt Charts ou biblioteca de gráficos compatível

## Backend

- Python
- FastAPI
- Pydantic
- SQLAlchemy
- Alembic
- PostgreSQL
- Uvicorn
- Gunicorn ou processo equivalente, se necessário
- Railway

## Segurança

- HTTPS
- JWT ou tokens de curta duração
- chaves de licença assinadas digitalmente
- hash SHA-256
- criptografia de backups
- armazenamento seguro de tokens
- autenticação por dispositivo

---

# 7. Arquitetura Interna do Desktop

Sugestão de estrutura:

```text
src/
│
├── core/
│   ├── application/
│   ├── domain/
│   ├── services/
│   └── validation/
│
├── modules/
│   ├── customers/
│   ├── products/
│   ├── sales/
│   ├── inventory/
│   ├── suppliers/
│   ├── finance/
│   ├── cash/
│   ├── reports/
│   ├── users/
│   └── settings/
│
├── infrastructure/
│   ├── database/
│   ├── api/
│   ├── backup/
│   ├── logging/
│   └── filesystem/
│
├── ui/
│   ├── qml/
│   ├── components/
│   ├── themes/
│   └── icons/
│
└── main.cpp
```

---

# 8. Banco Local

Banco:

**SQLite**

Motivos:

- leve;
- confiável;
- excelente para software desktop;
- não exige instalação de servidor;
- fácil de realizar backup;
- rápido;
- multiplataforma;
- suporte consolidado.

## Tabelas iniciais

```text
users
customers
suppliers
products
categories
product_variants
sales
sale_items
payments
cash_sessions
inventory_movements
inventory_counts
accounts_payable
accounts_receivable
expenses
settings
audit_logs
licenses
modules
backup_history
```

---

# 9. Módulos do ERP

# 9.1 Dashboard

Tela principal contendo:

- faturamento do dia;
- faturamento do mês;
- número de vendas;
- ticket médio;
- produtos com estoque baixo;
- produtos sem estoque;
- contas vencendo;
- contas vencidas;
- saldo do caixa;
- produtos mais vendidos;
- margem estimada;
- alertas.

Exemplo:

```text
Faturamento hoje       R$ 4.825,30
Vendas hoje            57
Ticket médio           R$ 84,65
Estoque crítico        12 produtos
Contas vencidas        3
Saldo do caixa         R$ 2.482,00
```

---

# 9.2 Clientes

Funcionalidades:

- nome;
- CPF/CNPJ opcional;
- telefone;
- WhatsApp;
- e-mail;
- endereço;
- data de nascimento;
- observações;
- histórico de compras;
- total gasto;
- última compra;
- limite de crédito futuro;
- status ativo/inativo.

Pesquisa por:

- nome;
- documento;
- telefone;
- código.

---

# 9.3 Produtos

Cadastro completo de produtos.

Campos:

- código interno;
- código de barras;
- descrição;
- categoria;
- marca;
- fornecedor;
- unidade;
- preço de custo;
- preço de venda;
- margem;
- estoque atual;
- estoque mínimo;
- estoque máximo;
- localização física;
- imagem;
- status;
- observações.

Recursos futuros:

- variantes;
- cor;
- tamanho;
- grade;
- múltiplos preços;
- atacado/varejo.

---

# 9.4 Estoque

Recursos:

- entrada;
- saída;
- ajuste;
- inventário;
- perda;
- avaria;
- devolução;
- transferência futura;
- histórico completo;
- estoque mínimo;
- alertas;
- custo médio;
- valorização do estoque.

Toda movimentação deverá gerar histórico.

Exemplo:

```text
Produto: Camiseta Preta M
Movimento: Venda
Quantidade: -2
Saldo anterior: 18
Saldo atual: 16
Usuário: João
Data: 19/09/2026 14:35
```

---

# 9.5 PDV

Tela otimizada para venda rápida.

Recursos:

- leitura de código de barras;
- pesquisa instantânea;
- quantidade;
- desconto;
- acréscimo;
- cliente opcional;
- diferentes formas de pagamento;
- cancelamento controlado;
- impressão;
- comprovante;
- finalização rápida.

Formas iniciais:

- dinheiro;
- PIX;
- cartão de crédito;
- cartão de débito;
- outros.

---

# 9.6 Caixa

Funcionalidades:

- abertura;
- fechamento;
- suprimento;
- sangria;
- vendas;
- recebimentos;
- despesas;
- conferência;
- histórico.

Relatório de fechamento:

```text
Abertura             R$ 200,00
Dinheiro             R$ 1.480,00
PIX                  R$ 2.150,00
Crédito              R$ 3.210,00
Débito               R$ 1.540,00
Sangrias             R$ 500,00
Saldo final          R$ ...
```

---

# 9.7 Fornecedores

Campos:

- razão social;
- nome fantasia;
- CNPJ;
- telefone;
- e-mail;
- endereço;
- contato;
- observações.

Recursos:

- histórico de compras;
- produtos fornecidos;
- valores;
- condições comerciais.

---

# 9.8 Financeiro

## Contas a pagar

- fornecedor;
- descrição;
- vencimento;
- valor;
- categoria;
- status;
- pagamento.

## Contas a receber

- cliente;
- venda;
- vencimento;
- valor;
- status.

## Despesas

Categorias:

- aluguel;
- energia;
- internet;
- salários;
- manutenção;
- compras;
- impostos;
- outras.

---

# 9.9 Relatórios

Relatórios iniciais:

- vendas por período;
- vendas por produto;
- vendas por categoria;
- produtos mais vendidos;
- produtos menos vendidos;
- estoque atual;
- estoque crítico;
- estoque parado;
- margem;
- faturamento;
- ticket médio;
- clientes que mais compram;
- contas a pagar;
- contas a receber;
- fluxo de caixa;
- fechamento de caixa.

Exportações:

- PDF;
- CSV;
- Excel quando aplicável.

---

# 10. Analytics Local

Um dos diferenciais do sistema será transformar dados em informações úteis.

Exemplos:

```text
"Produto X aumentou 22% em vendas nos últimos 30 dias."

"Produto Y está há 92 dias sem vender."

"Você possui R$ 8.450 em produtos com baixa movimentação."

"Com base no ritmo atual, o estoque do Produto Z termina em aproximadamente 9 dias."
```

Inicialmente esses cálculos podem ser realizados localmente.

Não é necessário IA para a primeira versão.

---

# 11. Interface e UX

A interface deve transmitir:

- profissionalismo;
- modernidade;
- simplicidade;
- velocidade.

## Design

Tecnologia:

**Qt Quick / QML**

Características:

- sidebar;
- cards;
- dashboards;
- tabelas modernas;
- dark mode futuro;
- ícones consistentes;
- boa tipografia;
- animações discretas;
- atalhos de teclado.

Exemplo:

```text
┌─────────────────────────────────────────┐
│ MH Store                   🔔   ⚙       │
├───────────────┬─────────────────────────┤
│ Dashboard     │ Faturamento hoje        │
│ PDV           │ R$ 4.825                │
│ Produtos      │                         │
│ Estoque       │ Vendas: 57              │
│ Clientes      │ Estoque baixo: 12       │
│ Fornecedores  │                         │
│ Financeiro    │ Ticket médio: R$ 84,65  │
│ Relatórios    │                         │
│ Configurações │                         │
└───────────────┴─────────────────────────┘
```

---

# 12. Sistema de Módulos

A aplicação deve nascer preparada para módulos.

Exemplo conceitual:

```cpp
enum class Module {
    POS,
    Inventory,
    Customers,
    Finance,
    Analytics,
    CloudBackup,
    MultiUser,
    CloudSync
};
```

Cada licença terá os módulos liberados.

Exemplo:

```json
{
  "license": "MH-9823-ABCD",
  "plan": "professional",
  "modules": {
    "pos": true,
    "inventory": true,
    "customers": true,
    "finance": false,
    "cloud_backup": true
  }
}
```

---

# 13. Backend MHSoftware

O backend não será responsável pelas operações do ERP.

Funções:

- licenciamento;
- autenticação;
- dispositivos;
- módulos;
- planos;
- versões;
- atualizações;
- backups;
- assinaturas;
- pagamentos futuros.

---

# 14. Endpoints Iniciais

Exemplos:

```text
POST /api/v1/auth/login

POST /api/v1/licenses/activate
POST /api/v1/licenses/validate

GET /api/v1/modules
GET /api/v1/version

POST /api/v1/devices/register

POST /api/v1/backups
GET  /api/v1/backups
POST /api/v1/backups/restore
```

---

# 15. Banco do Backend

PostgreSQL.

Tabelas iniciais:

```text
customers
licenses
devices
plans
modules
license_modules
subscriptions
versions
backup_metadata
payments
audit_logs
```

---

# 16. Licenciamento

## Modelo

O usuário compra uma licença.

Exemplo:

```text
MH-9K42-X7P2-QA81
```

Primeira ativação:

```text
ERP
 ↓
Servidor MHSoftware
 ↓
Valida licença
 ↓
Registra dispositivo
 ↓
Recebe licença assinada
 ↓
Salva localmente
```

O ERP continuará funcionando offline.

A licença será revalidada periodicamente.

Sugestão:

**30 dias.**

Falha de internet não bloqueia imediatamente o sistema.

---

# 17. Atualizações

Criar atualizador separado.

```text
MHStore.exe
MHUpdater.exe
```

Fluxo:

```text
ERP detecta atualização
        ↓
usuário autoriza
        ↓
MHStore fecha
        ↓
Updater baixa pacote
        ↓
verifica SHA-256
        ↓
instala
        ↓
abre nova versão
```

---

# 18. Backup Local

Disponível para todos.

Funções:

- backup manual;
- backup automático local;
- restore;
- escolha de pasta;
- histórico.

Formato possível:

```text
mhstore_backup_2026_09_19.mhb
```

O arquivo pode conter:

- SQLite;
- configurações;
- anexos;
- metadados.

---

# 19. MH Cloud Backup

Serviço premium.

Fluxo:

```text
SQLite
 ↓
snapshot consistente
 ↓
compressão
 ↓
criptografia
 ↓
upload
 ↓
armazenamento cloud
```

O usuário verá:

```text
MH Cloud Backup

Último backup:
19/09/2026 22:35

Status:
✓ Protegido

Backups disponíveis:
19/09
18/09
17/09

[ Restaurar ]
```

---

# 20. Armazenamento Cloud

O PostgreSQL não deve ser usado para armazenar grandes arquivos.

Utilizar:

## PostgreSQL

Para:

- cliente;
- licença;
- plano;
- status;
- data;
- tamanho;
- referência do arquivo.

## Object Storage

Para:

- backups;
- imagens;
- anexos;
- arquivos grandes.

---

# 21. Modelo Comercial

## Produto principal

### MH Store Starter

Pagamento único.

Exemplo inicial:

**R$ 199 a R$ 299**

Inclui:

- produtos;
- clientes;
- PDV;
- estoque;
- caixa;
- relatórios básicos;
- backup local.

---

## MH Store Professional

Exemplo:

**R$ 399 a R$ 499**

Inclui:

- financeiro;
- relatórios avançados;
- usuários;
- analytics;
- recursos adicionais.

---

# 22. Serviços Recorrentes

## MH Cloud

Exemplo:

**R$ 19,90/mês**

- backup automático;
- histórico;
- armazenamento seguro;
- restauração.

## MH Cloud Pro

Exemplo:

**R$ 39,90–59,90/mês**

Futuramente:

- sincronização;
- dashboard web;
- vários computadores;
- relatórios remotos;
- armazenamento maior.

---

# 23. Marketplace Interno

Tela opcional:

```text
Módulos MHSoftware

Financeiro Pro
R$ 99
[ Ativar ]

Analytics Pro
R$ 79
[ Ativar ]

MH Cloud
R$ 19,90/mês
[ Assinar ]

Multi-PC
R$ 149
[ Ativar ]
```

Sem publicidade invasiva.

---

# 24. Segurança

## Comunicação

Sempre:

```text
HTTPS
```

## Licença

Não armazenar apenas:

```text
license=true
```

A licença deve ser assinada pelo servidor.

## Backup

Antes do upload:

- criar snapshot;
- compactar;
- criptografar;
- calcular hash;
- enviar.

## Senhas

Nunca armazenar senha em texto puro.

Utilizar algoritmo moderno de hash de senha no backend, como:

- Argon2;
- bcrypt.

---

# 25. Logs e Auditoria

Registrar ações relevantes:

- venda cancelada;
- estoque alterado;
- produto excluído;
- desconto;
- fechamento de caixa;
- backup;
- login;
- alteração de configuração.

Exemplo:

```text
19/09/2026 15:42
Usuário: gerente
Ação: cancelamento de venda
Venda: #02832
Valor: R$ 289,90
```

---

# 26. Multiusuário

Primeira versão:

usuários locais.

Perfis:

- administrador;
- gerente;
- caixa;
- estoque.

Permissões:

```text
Cancelar venda
Editar produto
Alterar preço
Visualizar financeiro
Fechar caixa
Cadastrar usuário
Restaurar backup
```

---

# 27. Multi-PC Futuro

A primeira versão não deve tentar sincronizar vários PCs.

Posteriormente:

```text
PC Caixa 1
PC Caixa 2
PC Gerência
      ↓
MH Cloud Sync
      ↓
Banco sincronizado
```

Isso pode se tornar serviço premium.

---

# 28. Funcionalidades Futuras

Depois do MVP:

- emissão fiscal;
- NFC-e;
- NF-e;
- integração PIX;
- integração com WhatsApp;
- integração com Mercado Livre;
- integração com Shopee;
- integração com e-commerce;
- etiquetas;
- impressão térmica;
- balança;
- leitor de código de barras;
- dashboard web;
- aplicativo mobile;
- previsão de demanda;
- sugestão de compra;
- inteligência artificial.

---

# 29. O Que NÃO Fazer no MVP

Não começar com:

- NF-e;
- NFC-e;
- contabilidade;
- folha de pagamento;
- CRM complexo;
- e-commerce;
- marketplace;
- IA;
- sincronização em tempo real;
- aplicativo mobile;
- multi-filial complexo.

Isso aumenta o risco do projeto.

---

# 30. MVP

O primeiro produto vendável deverá conter:

```text
✓ Login
✓ Dashboard
✓ Clientes
✓ Produtos
✓ Categorias
✓ Fornecedores
✓ Estoque
✓ PDV
✓ Caixa
✓ Contas a pagar
✓ Contas a receber
✓ Relatórios básicos
✓ Backup local
✓ Configurações
✓ Licenciamento
```

---

# 31. Roadmap

## Fase 0 — Fundação

- repositório;
- CMake;
- estrutura modular;
- banco SQLite;
- logging;
- configuração;
- tema QML.

## Fase 1 — Cadastros

- usuários;
- produtos;
- categorias;
- clientes;
- fornecedores.

## Fase 2 — Estoque

- entradas;
- saídas;
- inventário;
- histórico;
- alertas.

## Fase 3 — PDV

- carrinho;
- código de barras;
- pagamentos;
- fechamento;
- impressão.

## Fase 4 — Financeiro

- contas a pagar;
- contas a receber;
- despesas.

## Fase 5 — Dashboard e Relatórios

- métricas;
- gráficos;
- PDF;
- CSV.

## Fase 6 — Licenciamento

- FastAPI;
- PostgreSQL;
- Railway;
- ativação;
- dispositivos.

## Fase 7 — Atualizações

- versionamento;
- updater;
- validação de pacote.

## Fase 8 — Cloud Backup

- criptografia;
- upload;
- restore;
- assinatura.

## Fase 9 — Comercialização

- instaladores;
- documentação;
- site;
- Mercado Livre;
- suporte;
- onboarding.

---

# 32. Estrutura do Projeto

```text
mh-store/
│
├── desktop/
│   ├── CMakeLists.txt
│   ├── src/
│   ├── qml/
│   ├── resources/
│   └── tests/
│
├── backend/
│   ├── app/
│   ├── migrations/
│   ├── tests/
│   ├── requirements.txt
│   └── Dockerfile
│
├── docs/
│   ├── architecture.md
│   ├── database.md
│   ├── api.md
│   └── product.md
│
└── README.md
```

---

# 33. Padrões de Desenvolvimento

Recomendações:

- Clean Architecture adaptada;
- Repository Pattern;
- Service Layer;
- DTOs;
- princípios SOLID;
- migrations;
- testes unitários;
- logging estruturado;
- controle de versão Git.

Evitar excesso de abstração no início.

---

# 34. Testes

## Desktop

Testar:

- regras de estoque;
- cálculo de venda;
- caixa;
- financeiro;
- migrations;
- backup;
- restore.

Ferramentas possíveis:

- Qt Test;
- Catch2;
- GoogleTest.

## Backend

- Pytest;
- testes da API;
- licenciamento;
- módulos;
- segurança;
- backup.

---

# 35. Distribuição

## Windows

Formatos:

```text
.exe
.msi
```

Criar instalador profissional.

## Linux

Formatos possíveis:

```text
AppImage
.deb
```

Prioridade comercial inicial:

**Windows**

Linux pode ser mantido como plataforma adicional.

---

# 36. Identidade Visual

Sugestão:

```text
MH Store
by MHSoftware
```

Interface:

- minimalista;
- corporativa;
- elegante;
- poucos elementos por tela;
- foco em velocidade.

Evitar visual parecido com ERP legado.

---

# 37. Jornada do Cliente

```text
Mercado Livre / Site
        ↓
Compra
        ↓
Recebe instalador
        ↓
Instala
        ↓
Informa licença
        ↓
Ativação
        ↓
Configuração inicial
        ↓
Cadastro / importação
        ↓
Uso offline
```

Depois:

```text
Usuário conhece MH Cloud
        ↓
ativa backup
        ↓
passa a pagar assinatura
```

---

# 38. Estratégia de Monetização

O objetivo não deve ser apenas vender um software.

A estratégia será:

```text
Venda inicial
       ↓
Cliente entra no ecossistema MHSoftware
       ↓
Compra módulos
       ↓
Assina serviços cloud
       ↓
Compra suporte / serviços adicionais
```

---

# 39. Posicionamento

Mensagem principal:

> MH Store é um ERP rápido e simples para quem quer cuidar da loja, e não cuidar do sistema.

Mensagens comerciais possíveis:

- "Funciona mesmo sem internet."
- "Seu estoque, suas vendas e seu caixa em um só lugar."
- "Sem mensalidade obrigatória."
- "Backup em nuvem opcional."
- "Windows e Linux."
- "Compre uma vez e use."

---

# 40. Visão de Longo Prazo

O MH Store pode se tornar uma plataforma.

```text
MH Store Desktop
       │
       ├── MH Cloud
       ├── MH Analytics
       ├── MH Sync
       ├── MH Fiscal
       ├── MH Marketplace
       ├── MH Mobile
       └── MH Integrations
```

O ERP local será o núcleo do ecossistema.

---

# 41. Decisão Técnica Recomendada

## Desktop

```text
C++20
Qt 6
Qt Quick/QML
SQLite
CMake
```

## Backend

```text
Python
FastAPI
Pydantic
SQLAlchemy
Alembic
PostgreSQL
Railway
```

## Modelo

```text
Offline-first
+
licença local
+
serviços online opcionais
```

---

# 42. Primeira Meta

A primeira meta não será construir o ERP completo.

Será entregar uma versão que permita:

```text
Cadastrar produto
Cadastrar cliente
Controlar estoque
Realizar venda
Abrir e fechar caixa
Visualizar faturamento
Gerar relatório
Fazer backup
```

Quando esse fluxo estiver excelente, os demais módulos serão adicionados.

---

# 43. Regra de Produto

Toda nova função deve responder:

1. Isso resolve um problema real do lojista?
2. Isso deixa o fluxo mais simples?
3. Isso pode funcionar offline?
4. Isso aumenta o valor percebido?
5. Isso deve fazer parte do produto ou ser módulo premium?

Se a resposta não estiver clara, a funcionalidade não entra imediatamente.

---

# 44. Resumo Executivo

O MH Store será um ERP desktop moderno, offline-first e multiplataforma.

O cliente poderá realizar toda a operação comercial localmente utilizando:

```text
C++ + Qt + SQLite
```

A infraestrutura online será responsável apenas por serviços complementares:

```text
FastAPI + PostgreSQL + Railway
```

A estratégia comercial será:

```text
licença permanente
+
módulos adicionais
+
serviços cloud opcionais
```

Isso permite à MHSoftware vender um produto acessível inicialmente e, posteriormente, aumentar o valor do cliente através de módulos e serviços recorrentes.

O projeto deve começar pequeno, com foco absoluto em:

**velocidade, estabilidade, simplicidade e excelente experiência de uso.**
