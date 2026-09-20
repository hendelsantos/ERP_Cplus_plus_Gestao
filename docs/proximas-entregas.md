# Próximas entregas

Revisão: 20/09/2026.

Este documento organiza as próximas entregas do MH Store considerando o modelo atual: uma instalação offline por cliente, com banco SQLite local e funcionamento independente de internet.

## Objetivo

Levar o sistema da fase funcional para uma distribuição confiável em clientes reais, preservando:

- funcionamento offline;
- dados locais sob controle do cliente;
- backups verificáveis;
- módulos adequados a diferentes tipos de negócio;
- instalação e suporte simplificados.

## Prioridade recomendada

1. Backup automático e recuperação.
2. Robustez operacional e diagnóstico.
3. Instalador e distribuição.
4. Demonstrações por segmento.
5. Recursos comerciais finais.
6. Licenciamento offline.

A ordem prioriza proteção dos dados antes de distribuição e monetização.

## 1. Backup automático

### Objetivo

Criar cópias locais periódicas do banco SQLite sem exigir uma ação manual do operador.

### Escopo

- backup automático ao iniciar o sistema, quando necessário;
- backup automático ao fechar o aplicativo ou o caixa;
- intervalo configurável;
- pasta de destino configurável;
- retenção dos últimos arquivos;
- validação de integridade após a criação;
- identificação de data, hora e versão do schema;
- restauração com confirmação e criação de cópia de segurança anterior.

### Critérios de conclusão

- o sistema cria backups sem interromper o fluxo normal;
- arquivos inválidos não são apresentados como válidos;
- backups antigos podem ser restaurados com migração controlada;
- falhas de disco ou permissão aparecem com mensagem clara;
- testes cobrem criação, retenção, corrupção e restauração.

## 2. Robustez e diagnóstico

### Objetivo

Facilitar suporte e recuperação sem expor credenciais ou dados sensíveis desnecessários.

### Escopo

- logging local de inicialização, migração, backup e falhas de transação;
- níveis de log configuráveis;
- rotação de arquivos de log;
- diagnóstico da versão do aplicativo, Qt, schema e caminho do banco;
- tratamento de desligamento inesperado;
- verificação de integridade do SQLite na inicialização;
- mensagens de erro com ação recomendada.

### Critérios de conclusão

- logs não contêm senhas, hashes, tokens ou códigos de recuperação;
- erros críticos possuem contexto suficiente para suporte;
- o aplicativo continua iniciando após falhas recuperáveis;
- testes simulam falhas de banco, disco e migração.

## 3. Instalador e distribuição

### Objetivo

Permitir que o sistema seja instalado e atualizado por clientes sem configuração manual do ambiente Qt.

### Escopo

- pacote `.deb` e/ou AppImage;
- executável, QML, plugins Qt e bibliotecas necessárias;
- atalho no menu do sistema;
- diretório de dados separado do diretório da aplicação;
- preservação do banco durante atualizações;
- verificação de versão mínima do banco;
- script de diagnóstico para suporte;
- instruções de instalação e atualização.

### Critérios de conclusão

- instalação limpa funciona em uma máquina sem Qt preparado;
- atualização preserva dados e executa migrações;
- desinstalação não remove o banco sem confirmação explícita;
- aplicativo inicia pelo menu do sistema;
- pacote é reproduzível a partir do projeto.

## 4. Demonstrações por segmento

### Objetivo

Preparar configurações e dados de exemplo para apresentar o sistema a diferentes negócios.

### Segmentos

- loja geral;
- moda;
- mercado;
- serviços;
- operação híbrida de produtos e serviços.

### Escopo

- perfil de módulos habilitados;
- categorias e produtos de exemplo;
- clientes e fornecedores fictícios;
- configuração de unidades e variações;
- serviço e ordem de serviço de demonstração;
- roteiro curto de operação para cada segmento;
- opção de limpar os dados de demonstração.

### Critérios de conclusão

- cada perfil pode ser criado sem alterar código;
- o fluxo principal de cada segmento é demonstrável;
- dados de exemplo são claramente identificados;
- a limpeza não remove dados reais sem confirmação;
- testes verificam as configurações e os fluxos básicos.

## 5. Recursos comerciais finais

### Objetivo

Concluir os recursos operacionais mais esperados em uma instalação de PDV offline.

### Escopo

- impressão física opcional do comprovante;
- configuração de impressora;
- relatório financeiro por período;
- conferência de vendas, caixa, despesas e recebíveis;
- exportação complementar de relatórios;
- fechamento operacional diário;
- filtros e indicadores por forma de pagamento;
- melhorias de usabilidade para operadores.

### Critérios de conclusão

- impressão não bloqueia o funcionamento sem impressora;
- relatórios podem ser conferidos contra os registros do SQLite;
- totais não duplicam pagamentos ou movimentos de caixa;
- fluxos de fechamento possuem testes de consistência;
- exportações continuam disponíveis quando a impressora não está configurada.

## 6. Licenciamento offline

### Objetivo

Controlar planos e período de avaliação sem exigir conexão contínua com a internet.

### Escopo

- chave de licença assinada;
- ativação manual por código;
- armazenamento local protegido;
- período de avaliação;
- identificação da edição e dos módulos permitidos;
- tolerância a alteração de relógio dentro de limites definidos;
- tela de status da licença;
- fluxo de renovação manual.

### Critérios de conclusão

- o aplicativo funciona offline após ativação;
- a licença não depende de um servidor para cada inicialização;
- chaves inválidas ou adulteradas são rejeitadas;
- o sistema não bloqueia dados do cliente de forma destrutiva;
- testes cobrem ativação, expiração, troca de máquina e recuperação.

## Fora do escopo imediato

Estas funcionalidades exigem projetos próprios antes de serem implementadas:

- emissão fiscal;
- integração com balança física;
- sincronização em nuvem;
- operação multiempresa no mesmo banco;
- aplicativo mobile;
- marketplace ou integração com plataformas externas.

## Definition of Done

Uma entrega só deve ser considerada concluída quando possuir:

- implementação no código;
- migração compatível, quando houver alteração de schema;
- teste automatizado do comportamento principal;
- teste de falha ou rollback quando houver transação;
- documentação atualizada;
- build e suíte completa aprovados;
- commit publicado no GitHub.
