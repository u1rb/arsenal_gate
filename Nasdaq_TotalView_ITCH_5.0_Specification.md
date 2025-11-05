# Nasdaq TotalView-ITCH 5.0

**ITCH is the revolutionary Nasdaq outbound protocol**

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Data Types](#data-types)
- [Message Formats](#message-formats)
  - [1.1 System Event Message](#11-system-event-message)
  - [1.2 Stock Related Messages](#12-stock-related-messages)
    - [1.2.1 Stock Directory](#121-stock-directory)
    - [1.2.2 Stock Trading Action](#122-stock-trading-action)
    - [1.2.3 Reg SHO Short Sale Price Test Restricted Indicator](#123-reg-sho-short-sale-price-test-restricted-indicator)
    - [1.2.4 Market Participant Position](#124-market-participant-position)
    - [1.2.5 Market-Wide Circuit Breaker (MWCB) Messaging](#125-market-wide-circuit-breaker-mwcb-messaging)
    - [1.2.6 Quoting Period Update](#126-quoting-period-update)
    - [1.2.7 Limit Up – Limit Down (LULD) Auction Collar](#127-limit-up--limit-down-luld-auction-collar)
    - [1.2.8 Operational Halt](#128-operational-halt)
  - [1.3 Add Order Message](#13-add-order-message)
    - [1.3.1 Add Order – No MPID Attribution](#131-add-order--no-mpid-attribution)
    - [1.3.2 Add Order with MPID Attribution](#132-add-order-with-mpid-attribution)
  - [1.4 Modify Order Messages](#14-modify-order-messages)
    - [1.4.1 Order Executed Message](#141-order-executed-message)
    - [1.4.2 Order Executed With Price Message](#142-order-executed-with-price-message)
    - [1.4.3 Order Cancel Message](#143-order-cancel-message)
    - [1.4.4 Order Delete Message](#144-order-delete-message)
    - [1.4.5 Order Replace Message](#145-order-replace-message)
  - [1.5 Trade Messages](#15-trade-messages)
    - [1.5.1 Trade Message (Non-Cross)](#151-trade-message-non-cross)
    - [1.5.2 Cross Trade Message](#152-cross-trade-message)
    - [1.5.3 Broken Trade / Order Execution Message](#153-broken-trade--order-execution-message)
  - [1.6 Net Order Imbalance Indicator (NOII) Message](#16-net-order-imbalance-indicator-noii-message)
  - [1.7 Retail Price Improvement Indicator (RPII)](#17-retail-price-improvement-indicator-rpii)
  - [1.8 Direct Listing with Capital Raise Price Discovery Message](#18-direct-listing-with-capital-raise-price-discovery-message)
- [Appendix A - Documentation and Revision Control Log](#appendix-a---documentation-and-revision-control-log)
- [Appendix B – Stock Symbol Convention](#appendix-b--stock-symbol-convention)
- [Appendix C – Trading Action Reason Codes](#appendix-c--trading-action-reason-codes)
- [Appendix D – Issue Classification Values](#appendix-d--issue-classification-values)
- [Appendix E – Issue Sub Type Values](#appendix-e--issue-sub-type-values)

---

## Overview

Nasdaq TotalView ITCH is a direct data feed product offered by The Nasdaq Stock Market, LLC. This specification covers both the software and hardware (FPGA) versions of the feed.

Nasdaq TotalView ITCH features the following data elements (in binary number format) for all exchange-listed equities securities traded via the Nasdaq execution system:

### Order level data with attribution
For Nasdaq execution system, Nasdaq will provide its full order depth using the standard ITCH format. TotalView ITCH uses a series of messages to track the life of a customer order.

TOTALVIEW-ITCH is an outbound market data feed only. As an added feature, the TotalView-ITCH message formats will support Nasdaq market participant attribution. TotalView-ITCH carries order level data for NYSE, NYSE American, NYSE Arca, and BATS listed securities as well as for Nasdaq-listed securities.

### Trade messages
To ensure that customers have complete information about Nasdaq order flow, TotalView-ITCH supports a trade message to reflect a match of a non-displayable order in the Nasdaq system. TotalView-ITCH also supports a separate trade message to reflect Nasdaq cross transactions.

### Net Order Imbalance Data
In the minutes leading up to the Nasdaq Opening and Closing Crosses as well as the Nasdaq Crosses for IPO or halted/paused securities, Nasdaq disseminates the indicative clearing price and net order imbalance on Nasdaq. Because the calculation includes non-displayable as well as displayable order types, the Net Order Imbalance Indicator (NOII) is the best predictor of the Nasdaq opening and closing prices available to the public.

### Administrative messages
Such as trading actions and symbol directory messages:

- **Trading action messages** are used to inform market participants when a security is halted/paused or released for trading.
- **Symbol Directory messages** provide basic security data such as the market tier and Financial Status Indicator.
- **Market participant position message** carries the Primary Market Maker status, Market Participant status and Market Maker mode fields used by some firms to comply with market regulations.

### Event controls
Such as start of day, end of day and emergency market halt/resume.

---

## Architecture

The TotalView ITCH feed is made up of a series of sequenced messages. Each message is variable in length based on the message type. The messages that make up the TotalView ITCH protocol are typically delivered using a higher level protocol that takes care of sequencing and delivery guarantees.

Nasdaq offers the TotalView ITCH data feed in three protocol options:

- SoupBinTCP
- Compressed via SoupBinTCP
- MoldUDP64

In the market data messages, instruments are identified by a **stock locate code** – a low lying integer employed with the intent of serving as an array index for rapidly looking up instrument details. The locate codes are dynamically assigned each day, starting with a value of 1, and communicated via the Stock Directory message. An instrument's locate code will not change intraday; however, there should be no expectation that the assignment will be the same across multiple days. The Stock Locate code appears in all messages, and at the same position in all messages to support efficient filtering. A default value of 0 will be applied to messages which are not stock dependent.

### FPGA version

Nasdaq will broadcast the TotalView ITCH FPGA feed from the U.S. primary data center facility in Carteret, New Jersey in the MoldUDP64 protocol option only. Given the unshaped network traffic, Nasdaq is requiring firms to have 10 Gb or 40 Gb network connection into the Carteret, NJ data center to obtain the TotalView ITCH FPGA feed.

As with the software version of the feed, the TotalView ITCH FPGA feed will be comprised of a series of sequenced order messages. Outside of the fact that the FPGA data delivery is unthrottled or unshaped at the network level, the TotalView-ITCH payload will be the same for both versions of the TotalView-ITCH 5.0 data formats. TotalView ITCH FPGA product is guaranteed to disseminate payload messages in the same exact order as the software-based version of the TotalView ITCH feed.

With this messaging sequencing guarantee, TotalView ITCH FPGA firms will be able to utilize the GLIMPSE service to obtain current state of the book retransmissions. Firms may also use the software version of TotalView ITCH for fault tolerance or disaster recovery purposes.

---

## Data Types

All integer fields are **big-endian** (network byte order) binary encoded numbers. Unless otherwise noted, they are unsigned.

All alpha fields are **ASCII** fields which are left justified and padded on the right with spaces.

**Prices** are integer fields, supplied with an associated precision. When converted to a decimal format, prices are in fixed point format, where the precision defines the number of decimal places. For example, a field flagged as Price (4) has an implied 4 decimal places. The maximum value of price (4) in TotalView ITCH is 200,000.0000 (decimal, 77359400 hex).

**Timestamps** are represented as nanoseconds since midnight.

---

## Message Formats

The TotalView ITCH feed is composed of a series of messages that describe orders added to, removed from, and executed on Nasdaq as well as disseminate Cross and Stock Directory information.

### 1.1 System Event Message

The system event message type is used to signal a market or data feed handler event. The format is as follows:

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "S" | System Event Message |
| Stock Locate | 1 | 2 | Integer | Always 0 |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Event Code | 11 | 1 | Alpha | See System Event Codes below |

#### System Event Codes – Daily

| Code | Explanation |
|------|-------------|
| "O" | **Start of Messages.** Outside of time stamp messages, the start of day message is the first message sent in any trading day. |
| "S" | **Start of System hours.** This message indicates that NASDAQ is open and ready to start accepting orders. |
| "Q" | **Start of Market hours.** This message is intended to indicate that Market Hours orders are available for execution. |
| "M" | **End of Market hours.** This message is intended to indicate that Market Hours orders are no longer available for execution. |
| "E" | **End of System hours.** It indicates that Nasdaq is now closed and will not accept any new orders today. It is still possible to receive Broken Trade messages and Order Delete messages after the End of Day. |
| "C" | **End of Messages.** This is always the last message sent in any trading day. |

---

### 1.2 Stock Related Messages

#### 1.2.1 Stock Directory

At the start of each trading day, Nasdaq disseminates stock directory messages for all active symbols in the Nasdaq execution system.

Market data redistributors should process this message to populate the Financial Status Indicator (required display field) and the Market Category (recommended display field) for Nasdaq listed issues.

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| Message Type | 0 | 1 | "R" | Stock Directory Message |
| Stock Locate | 1 | 2 | Integer | Locate Code uniquely assigned to the security symbol for the day. |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Time at which the directory message was generated. Refer to Data Types for field processing notes. |
| Stock | 11 | 8 | Alpha | Denotes the security symbol for the issue in the Nasdaq execution system. |
| Market Category | 19 | 1 | Alpha | Indicates Listing market or listing market tier for the issue |

**Market Category Codes:**

**Nasdaq-Listed Instruments:**
- Q = Nasdaq Global Select Market℠
- G = Nasdaq Global Market℠
- S = Nasdaq Capital Market®

**Non-Nasdaq-Listed Instruments:**
- N = New York Stock Exchange (NYSE)
- A = NYSE American
- P = NYSE Arca
- Z = BATS Z Exchange
- V = Investors' Exchange, LLC
- <space> = Not available

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| Financial Status Indicator | 20 | 1 | Alpha | For Nasdaq listed issues, this field indicates when a firm is not in compliance with Nasdaq continued listing requirements |

**Financial Status Indicator Codes:**

**Nasdaq-Listed Instruments:**
- D = Deficient
- E = Delinquent
- Q = Bankrupt
- S = Suspended
- G = Deficient and Bankrupt
- H = Deficient and Delinquent
- J = Delinquent and Bankrupt
- K = Deficient, Delinquent and Bankrupt
- C = Creations and/or Redemptions Suspended for Exchange Traded Product
- N = Normal (Default): Issuer Is NOT Deficient, Delinquent, or Bankrupt

**Non-Nasdaq-Listed Instruments:**
- <space> = Not available. Firms should refer to SIAC feeds for code if needed.

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| Round Lot Size | 21 | 4 | Integer | Denotes the number of shares that represent a round lot for the issue |
| Round Lots Only | 25 | 1 | Alpha | Indicates if Nasdaq system limits order entry for issue |

**Round Lots Only Codes:**
- Y = Nasdaq system only accepts round lots
- N = Nasdaq system does not have any order size restrictions for this security. Odd and mixed lot orders are allowed.

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| Issue Classification | 26 | 1 | Alpha | Identifies the security class for the issue as assigned by Nasdaq. See Appendix for allowable values. |
| Issue Sub-Type | 27 | 2 | Alpha | Identifies the security sub-type for the issue as assigned by Nasdaq. See Appendix for allowable values. |
| Authenticity | 29 | 1 | Alpha | Denotes if an issue or quoting participant record is set-up in Nasdaq systems in a live/production, test, or demo state. Please note that firms should only show live issues and quoting participants on public quotation displays. |

**Authenticity Codes:**
- P = Live/Production
- T = Test

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| Short Sale Threshold Indicator | 30 | 1 | Alpha | Indicates if a security is subject to mandatory close-out of short sales under SEC Rule 203(b)(3). |

**Short Sale Threshold Indicator Codes:**
- Y = Issue is restricted under SEC Rule 203(b)(3)
- N = Issue is not restricted
- <space> = Threshold Indicator not available

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| IPO Flag | 31 | 1 | Alpha | Indicates if the Nasdaq security is set up for IPO release. This field is intended to help Nasdaq market participant firms comply with FINRA Rule 5131(b). |

**IPO Flag Codes:**

**Nasdaq-Listed Instruments:**
- Y = Nasdaq listed instrument is set up as a new IPO security
- N = Nasdaq listed instrument is not set up as a new IPO security

**Non-Nasdaq-Listed Instruments:**
- <space> = Not available

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| LULD Reference Price Tier | 32 | 1 | Alpha | Indicates which Limit Up / Limit Down price band calculation parameter is to be used for the instrument. Refer to LULD Rule for details. |

**LULD Reference Price Tier Codes:**
- 1 = Tier 1 NMS Stocks and select ETPs
- 2 = Tier 2 NMS Stocks
- <space> = Not available

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| ETP Flag | 33 | 1 | Alpha | Indicates whether the security is an exchange traded product (ETP) |

**ETP Flag Codes:**
- Y = Instrument is an ETP
- N = Instrument is not an ETP
- <space> = Not available

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| ETP Leverage Factor | 34 | 4 | Integer | Tracks the integral relationship of the ETP to the underlying index. Example: If the underlying Index increases by a value of 1 and the ETP's Leverage factor is 3, indicates the ETF will increase/decrease (see Inverse) by 3. Leverage Factor is rounded to the nearest integer below, e.g. leverage factor 1 would represent leverage factors of 1 to 1.99. This field is used for LULD Tier I price band calculation purposes. |
| Inverse Indicator | 38 | 1 | Alpha | Indicates the directional relationship between the ETP and Underlying index. |

**Inverse Indicator Codes:**
- Y = ETP is an Inverse ETP
- N = ETP is not an Inverse ETP

Example: An ETP Leverage Factor of 3 and an Inverse value of 'Y' indicates the ETP will decrease by a value of 3.

---

#### 1.2.2 Stock Trading Action

Nasdaq uses this administrative message to indicate the current trading status of a security to the trading community.

Prior to the start of system hours, Nasdaq will send out a Trading Action spin. In the spin, Nasdaq will send out a Stock Trading Action message with the "T" (Trading Resumption) for all Nasdaq- and other exchange-listed securities that are eligible for trading at the start of the system hours. If a security is absent from the pre-opening Trading Action spin, firms should assume that the security is being treated as halted in the Nasdaq platform at the start of the system hours. Please note that securities may be halted in the Nasdaq system for regulatory or operational reasons.

After the start of system hours, Nasdaq will use the Trading Action message to relay changes in trading status for an individual security. Messages will be sent when a stock is:

- Halted
- Paused*
- Released for quotation
- Released for trading

*The paused status will be disseminated for NASDAQ-listed securities only. Trading pauses on non-NASDAQ listed securities will be treated simply as a halt.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "H" | Stock Trading Action Message. |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Stock | 11 | 8 | Alpha | Stock symbol, right padded with spaces |
| Trading State | 19 | 1 | Alpha | Indicates the current trading state for the stock. Allowable values: "H" = Halted across all U.S. equity markets / SROs; "P" = Paused across all U.S. equity markets / SROs (Nasdaq-listed securities only); "Q" = Quotation only period for cross-SRO halt or pause; "T" = Trading on Nasdaq |
| Reserved | 20 | 1 | Alpha | Reserved. |
| Reason | 21 | 4 | Alpha | Trading Action reason. |

---

#### 1.2.3 Reg SHO Short Sale Price Test Restricted Indicator

In February 2011, the Securities and Exchange Commission (SEC) implemented changes to Rule 201 of the Regulation SHO (Reg SHO). For details, please refer to SEC Release Number 34-61595. In association with the Reg SHO rule change, Nasdaq will introduce the following Reg SHO Short Sale Price Test Restricted Indicator message format.

For Nasdaq-listed issues, Nasdaq supports a full pre-opening spin of Reg SHO Short Sale Price Test Restricted Indicator messages indicating the Rule 201 status for all active issues. Nasdaq also sends the Reg SHO Short Sale Price Test Restricted Indicator message in the event of an intraday status change.

For other exchange-listed issues, Nasdaq relays the Reg SHO Short Sale Price Test Restricted Indicator message when it receives an update from the primary listing exchange.

Nasdaq processes orders based on the most Reg SHO Restriction status value.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "Y" | Reg SHO Short Sale Price Test Restricted Indicator |
| Locate Code | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Stock | 11 | 8 | Alpha | Stock symbol, right padded with spaces |
| Reg SHO Action | 19 | 1 | Alpha | Denotes the Reg SHO Short Sale Price Test Restriction status for the issue at the time of the message dissemination. Allowable values are: "0" = No price test in place; "1" = Reg SHO Short Sale Price Test Restriction in effect due to an intra-day price drop in security; "2" = Reg SHO Short Sale Price Test Restriction remains in effect |

---

#### 1.2.4 Market Participant Position

At the start of each trading day, Nasdaq disseminates a spin of market participant position messages. The message provides the Primary Market Maker status, Market Maker mode and Market Participant state for each Nasdaq market participant firm registered in an issue. Market participant firms may use these fields to comply with certain marketplace rules.

Throughout the day, Nasdaq will send out this message only if Nasdaq Operations changes the status of a market participant firm in an issue.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "L" | Market Participant Position message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| MPID | 11 | 4 | Alpha | Denotes the market participant identifier for which the position message is being generated |
| Stock | 15 | 8 | Alpha | Stock symbol, right padded with spaces |
| Primary Market Maker | 23 | 1 | Alpha | Indicates if the market participant firm qualifies as a Primary Market Maker in accordance with Nasdaq marketplace rules. "Y" = primary market maker; "N" = non-primary market maker |
| Market Maker Mode | 24 | 1 | Alpha | Indicates the quoting participant's registration status in relation to SEC Rules 101 and 104 of Regulation M. "N" = normal; "P" = passive; "S" = syndicate; "R" = pre-syndicate; "L" = penalty |
| Market Participant State | 25 | 1 | Alpha | Indicates the market participant's current registration status in the issue. "A" = Active; "E" = Excused/Withdrawn; "W" = Withdrawn; "S" = Suspended; "D" = Deleted |

---

#### 1.2.5 Market-Wide Circuit Breaker (MWCB) Messaging

##### 1.2.5.1 MWCB Decline Level Message

Informs data recipients what the daily MWCB breach points are set to for the current trading day.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "V" | Market wide circuit breaker Decline Level Message. |
| Stock Locate | 1 | 2 | Integer | Always set to 0 |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Time at which the MWCB Decline Level message was generated |
| Level 1 | 11 | 8 | Price (8) | Denotes the MWCB Level 1 Value. |
| Level 2 | 19 | 8 | Price (8) | Denotes the MWCB Level 2 Value. |
| Level 3 | 27 | 8 | Price (8) | Denotes the MWCB Level 3 Value. |

##### 1.2.5.2 MWCB Status Message

Informs data recipients when a MWCB has breached one of the established levels.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "W" | Market-Wide Circuit Breaker Status message |
| Stock Locate | 1 | 2 | Integer | Always set to 0 |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Time at which the MWCB Breaker Status message was generated |
| Breached Level | 11 | 1 | Alpha | Denotes the MWCB Level that was breached. "1" = Level 1; "2" = Level 2; "3" = Level 3 |

---

#### 1.2.6 Quoting Period Update

Indicates the anticipated IPO quotation release time of a security.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "K" | IPO Quoting Period Update Message |
| Stock Locate | 1 | 2 | Integer | Always set to 0 |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Time at which the IPO Quoting Period Update message was generated |
| Stock | 11 | 8 | Alpha | Stock symbol, right padded with spaces |
| IPO Quotation Release Time | 19 | 4 | Integer | Denotes the IPO release time, in seconds since midnight, for quotation to the nearest second. NOTE: If the quotation period is being canceled/postponed, we should state that: 1. IPO Quotation Time will be set to 0; 2. IPO Price will be set to 0 |
| IPO Quotation Release Qualifier | 23 | 1 | Alpha | "A" = Anticipated Quotation Release Time: This value would be used when Nasdaq Market Operations initially enters the IPO instrument for release; "C" = IPO Release Canceled/Postponed: This value would be used when Nasdaq Market Operations cancels or postpones the release of the new IPO instrument |
| IPO Price | 24 | 4 | Price (4) | Denotes the IPO Price to be used for intraday net change calculations. Prices are given in decimal format with 6 whole number places followed by 4 decimal digits. The whole number portion is padded on the left with spaces; the decimal portion is padded on the right with zeroes. The decimal point is implied by position, it does not appear inside the price field |

---

#### 1.2.7 Limit Up – Limit Down (LULD) Auction Collar

Indicates the auction collar thresholds within which a paused security can reopen following a LULD Trading Pause.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "J" | LULD Auction Collar |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds past midnight |
| Stock | 11 | 8 | Alpha | Stock symbol, right padded with spaces |
| Auction Collar Reference Price | 19 | 4 | Price (4) | Reference price used to set the Auction Collars |
| Upper Auction Collar Price | 23 | 4 | Price (4) | Indicates the price of the Upper Auction Collar Threshold |
| Lower Auction Collar Price | 27 | 4 | Price (4) | Indicates the price of the Lower Auction Collar Threshold |
| Auction Collar Extension | 31 | 4 | Integer | Indicates the number of the extensions to the Reopening Auction |

---

#### 1.2.8 Operational Halt

The Exchange uses this message to indicate the current Operational Status of a security to the trading community. An Operational Halt means that there has been an interruption of service on the identified security impacting only the designated Market Center. These Halts differ from the "Stock Trading Action" message types since an Operational Halt is specific to the exchange for which it is declared, and does not interrupt the ability of the trading community to trade the identified instrument on any other marketplace.

Nasdaq uses this administrative message to indicate the current trading status of the three market centers operated by Nasdaq.

| Name | Offset | Length | Type | Value/Description |
|------|--------|--------|------|-------------------|
| Message Type | 0 | 1 | "h" | Operational Halt |
| Stock Locate | 1 | 2 | Integer | Locate code uniquely assigned to the security symbol for the day. |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Time at which the Operational Halt message was generated. Refer to Data Types for field processing notes. |
| Stock | 11 | 8 | Alpha | Denotes the security symbol for the issue in Nasdaq execution system |
| Market Code | 19 | 1 | Alpha | "Q": Nasdaq; "B": BX; "X": PSX |
| Operational Halt Action | 20 | 1 | Alpha | "H": Operationally Halted on the identified Market; "T": Operational Halt has been lifted and Trading resumed |

---

### 1.3 Add Order Message

An Add Order Message indicates that a new order has been accepted by the Nasdaq system and was added to the displayable book. The message includes a day-unique Order Reference Number used by Nasdaq to track the order. Nasdaq will support two variations of the Add Order message format.

#### 1.3.1 Add Order – No MPID Attribution

This message will be generated for unattributed orders accepted by the Nasdaq system. (Note: If a firm wants to display a MPID for unattributed orders, Nasdaq recommends that it use the MPID of "NSDQ".)

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "A" | Add Order – No MPID Attribution Message. |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight. |
| Order Reference Number | 11 | 8 | Integer | The unique reference number assigned to the new order at the time of receipt. |
| Buy/Sell Indicator | 19 | 1 | Alpha | The type of order being added. "B" = Buy Order. "S" = Sell Order. |
| Shares | 20 | 4 | Integer | The total number of shares associated with the order being added to the book. |
| Stock | 24 | 8 | Alpha | Stock symbol, right padded with spaces |
| Price | 32 | 4 | Price (4) | The display price of the new order. Refer to Data Types for field processing notes. |

#### 1.3.2 Add Order with MPID Attribution

This message will be generated for attributed orders and quotations accepted by the Nasdaq system.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "F" | Add Order – MPID Attribution Message. |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight. |
| Order Reference Number | 11 | 8 | Integer | The unique reference number assigned to the new order at the time of receipt. |
| Buy/Sell Indicator | 19 | 1 | Alpha | The type of order being added. "B" = buy order. "S" = sell order. |
| Shares | 20 | 4 | Integer | The total number of shares associated with the order being added to the book |
| Stock | 24 | 8 | Alpha | Stock symbol, right padded with spaces |
| Price | 32 | 4 | Price (4) | The display price of the new order. Refer to Data Types for field processing notes. |
| Attribution | 36 | 4 | Alpha | Nasdaq Market participant identifier associated with the entered order |

---

### 1.4 Modify Order Messages

Modify Order messages always include the Order Reference Number of the Add Order to which the update applies. To determine the current display shares for an order, ITCH subscribers must deduct the number of shares stated in the Modify message from the original number of shares stated in the Add Order message with the same reference number. Nasdaq may send multiple Modify Order messages for the same order reference number and the effects are cumulative. When the number of display shares for an order reaches zero, the order is dead and should be removed from the book.

#### 1.4.1 Order Executed Message

This message is sent whenever an order on the book is executed in whole or in part. It is possible to receive several Order Executed Messages for the same order reference number if that order is executed in several parts. The multiple Order Executed Messages on the same order are cumulative.

By combining the executions from both types of Order Executed Messages and the Trade Message, it is possible to build a complete view of all non-cross executions that happen on Nasdaq. Cross execution information is available in one bulk print per symbol via the Cross Trade Message.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "E" | Add Order – Order Executed Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Order Reference Number | 11 | 8 | Integer | The unique reference number assigned to the new order at the time of receipt |
| Executed Shares | 19 | 4 | Integer | The number of shares executed |
| Match Number | 23 | 8 | Integer | The Nasdaq generated day unique Match Number of this execution. The Match Number is also referenced in the Trade Break Message |

#### 1.4.2 Order Executed With Price Message

This message is sent whenever an order on the book is executed in whole or in part at a price different from the initial display price. Since the execution price is different than the display price of the original Add Order, Nasdaq includes a price field within this execution message.

It is possible to receive multiple Order Executed and Order Executed With Price messages for the same order if that order is executed in several parts. The multiple Order Executed messages on the same order are cumulative.

These executions may be marked as non-printable. If the execution is marked as non-printed, it means that the shares will be included into a later bulk print (e.g., in the case of cross executions). If a firm is looking to use the data in time-and-sales displays or volume calculations, Nasdaq recommends that firms ignore messages marked as non-printable to prevent double counting.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "C" | Add Order – Order Executed with Price Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Order Reference Number | 11 | 8 | Integer | The unique reference number assigned to the new order at the time of receipt |
| Executed Shares | 19 | 4 | Integer | The number of shares executed |
| Match Number | 23 | 8 | Integer | The Nasdaq generated day unique Match Number of this execution. The Match Number is also referenced in the Trade Break Message |
| Printable | 31 | 1 | Alpha | Indicates if the execution should be reflected on time and sales displays and volume calculations. "N" = Non-Printable; "Y" = Printable |
| Execution Price | 32 | 4 | Price(4) | The Price at which the order execution occurred. Refer to Data Types for field processing notes |

#### 1.4.3 Order Cancel Message

This message is sent whenever an order on the book is modified as a result of a partial cancellation.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "X" | Order Cancel Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Order Reference Number | 11 | 8 | Integer | The reference number of the order being canceled |
| Cancelled Shares | 19 | 4 | Integer | The number of shares being removed from the display size of the order as a result of a cancellation |

#### 1.4.4 Order Delete Message

This message is sent whenever an order on the book is being cancelled. All remaining shares are no longer accessible so the order must be removed from the book.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "D" | Order Delete Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Order Reference Number | 11 | 8 | Integer | The reference number of the order being canceled |

#### 1.4.5 Order Replace Message

This message is sent whenever an order on the book has been cancel-replaced. All remaining shares from the original order are no longer accessible, and must be removed. The new order details are provided for the replacement, along with a new order reference number which will be used henceforth. Since the side, stock symbol and attribution (if any) cannot be changed by an Order Replace event, these fields are not included in the message. Firms should retain the side, stock symbol and MPID from the original Add Order message.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "U" | Order Replace Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Original Order Reference Number | 11 | 8 | Integer | The original order reference number of the order being replaced |
| New Order Reference Number | 19 | 8 | Integer | The new reference number for this order at time of replacement. Please note that the Nasdaq system will use this new order reference number for all subsequent updates |
| Shares | 27 | 4 | Integer | The new total displayed quantity |
| Price | 31 | 4 | Price (4) | The new display price for the order. Please refer to Data Types for field processing notes |

---

### 1.5 Trade Messages

#### 1.5.1 Trade Message (Non-Cross)

The Trade Message is designed to provide execution details for normal match events involving non-displayable order types. (Note: There is a separate message for Nasdaq cross events.)

Since no Add Order Message is generated when a non-displayed order is initially received, Nasdaq cannot use the Order Executed messages for all matches. Therefore this message indicates when a match occurs between non-displayable order types. A Trade Message is transmitted each time a non-displayable order is executed in whole or in part. It is possible to receive multiple Trade Messages for the same order if that order is executed in several parts. Trade Messages for the same order are cumulative.

Trade Messages should be included in Nasdaq time-and-sales displays as well as volume and other market statistics. Since Trade Messages do not affect the book, however, they may be ignored by firms just looking to build and track the Nasdaq execution system display.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "P" | Trade Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight. |
| Order Reference Number | 11 | 8 | Integer | The unique reference number assigned to the order on the book being executed. Effective December 6, 2010, Nasdaq will populate the Order Reference Number field within the Trade (Non-Cross) message as zero. For the binary versions of the TotalView-ITCH data feeds, the field will be null-filled bytes (which encodes sequence of zero) |
| Buy/Sell Indicator | 19 | 1 | Alpha | The type of non-display order on the book being matched. "B" = Buy Order; "S" = Sell Order. Effective 07/14/2014, this field will always be "B" regardless of the resting side |
| Shares | 20 | 4 | Integer | The number of shares being matched in this execution |
| Stock | 24 | 8 | Alpha | Stock Symbol, right padded with spaces |
| Price | 32 | 4 | Price (4) | The match price of the order. Please refer to Data Types for field processing notes |
| Match Number | 36 | 8 | Integer | The Nasdaq generated session unique Match Number for this trade. The Match Number is referenced in the Trade Break Message |

#### 1.5.2 Cross Trade Message

Cross Trade message indicates that Nasdaq has completed its cross process for a specific security. Nasdaq sends out a Cross Trade message for all active issues in the system following the Opening, Closing and EMC cross events. Firms may use the Cross Trade message to determine when the cross for each security has been completed. (Note: For the halted / paused securities, firms should use the Trading Action message to determine when an issue has been released for trading.)

For most issues, the Cross Trade message will indicate the bulk volume associated with the cross event. If the order interest is insufficient to conduct a cross in a particular issue, however, the Cross Trade message may show the shares as zero.

To avoid double counting of cross volume, firms should not include transactions marked as non-printable in time-and-sales displays or market statistic calculations.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "Q" | Cross Trade Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight. |
| Shares | 11 | 8 | Integer | The number of shares matched in the Nasdaq Cross. |
| Stock | 19 | 8 | Alpha | Stock symbol, right padded with spaces |
| Cross Price | 27 | 4 | Price (4) | The price at which the cross occurred. Refer to Data Types for field processing notes. |
| Match Number | 31 | 8 | Integer | The Nasdaq generated day-unique Match Number of this execution. |
| Cross Type | 39 | 1 | Alpha | The Nasdaq cross session for which the message is being generated. "O" = Nasdaq Opening Cross. "C" = Nasdaq Closing Cross. "H" = Cross for IPO and halted / paused securities. |

#### 1.5.3 Broken Trade / Order Execution Message

The Broken Trade Message is sent whenever an execution on Nasdaq is broken. An execution may be broken if it is found to be "clearly erroneous" pursuant to Nasdaq's Clearly Erroneous Policy. A trade break is final; once a trade is broken, it cannot be reinstated.

Firms that use the ITCH feed to create time-and-sales displays or calculate market statistics should be prepared to process the broken trade message. If a firm is only using the ITCH feed to build a book, however, it may ignore these messages as they have no impact on the current book.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "B" | Broken Trade Message. |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight. |
| Match Number | 11 | 8 | Integer | The Nasdaq Match Number of the execution that was broken. This refers to a Match Number from a previously transmitted Order Executed Message, Order Executed With Price Message, or Trade Message. |

---

### 1.6 Net Order Imbalance Indicator (NOII) Message

- Nasdaq begins disseminating Net Order Imbalance Indicators (NOII) at 9:25 a.m. for the Opening Cross and 3:50 p.m. for the Closing Cross.
- Between 9:25 and 9:28 a.m. and 3:50 and 3:55 p.m., Nasdaq disseminates the NOII information every 10 seconds.
- Between 9:28 and 9:30 a.m. and 3:55 and 4:00 p.m., Nasdaq disseminates the NOII information every second.
- For Nasdaq Halt, IPO and Pauses, NOII messages will be disseminated at 1 second intervals starting 1 second after quoting period starts/trading action is released.
- For more information, please see the FAQ on Opening and Closing Crosses.
- Nasdaq will also disseminate an Extended Trading Close (ETC) message from 4:00 p.m. to 4:05 p.m. at five second intervals.
- For more information, please see the FAQ on Extended Trading Close.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "I" | NOII Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight. |
| Paired Shares | 11 | 8 | Integer | The total number of shares that are eligible to be matched at the Current Reference Price. |
| Imbalance Shares | 19 | 8 | Integer | The number of shares not paired at the Current Reference Price. |
| Imbalance Direction | 27 | 1 | Alpha | The market side of the order imbalance. "B" = buy imbalance; "S" = sell imbalance; "N" = no imbalance; "O" = Insufficient orders to calculate; "P" = Paused |
| Stock | 28 | 8 | Alpha | Stock symbol, right padded with spaces |
| Far Price | 36 | 4 | Price (4) | A hypothetical auction-clearing price for cross orders only. Refer to Data Types for field processing notes. |
| Near Price | 40 | 4 | Price (4) | A hypothetical auction-clearing price for cross orders as well as continuous orders. Refer to Data Types for field |
| Current Reference Price | 44 | 4 | Price (4) | The price at which the NOII shares are being calculated. Refer to Data Types for field processing notes. |
| Cross Type | 48 | 1 | Alpha | The type of Nasdaq cross for which the NOII message is being generated. "O" = Nasdaq Opening Cross; "C" = Nasdaq Closing Cross; "H" = Cross for IPO and halted / paused securities; "A" = Extended Trading Close |
| Price Variation Indicator | 49 | 1 | Alpha | This field indicates the absolute value of the percentage of deviation of the Near Indicative Clearing Price to the nearest Current Reference Price. "L" = Less than 1%; "1" = 1 to 1.99%; "2" = 2 to 2.99%; "3" = 3 to 3.99%; "4" = 4 to 4.99%; "5" = 5 to 5.99%; "6" = 6 to 6.99%; "7" = 7 to 7.99%; "8" = 8 to 8.99%; "9" = 9 to 9.99%; "A" = 10 to 19.99%; "B" = 20 to 29.99%; "C" = 30% or greater; <Space> = Cannot be calculated |

---

### 1.7 Retail Price Improvement Indicator (RPII)

Identifies a retail interest indication of the Bid, Ask or both the Bid and Ask for Nasdaq-listed securities.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "N" | Retail Interest message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight. |
| Stock | 11 | 8 | Alpha | Stock symbol, right padded with spaces |
| Interest Flag | 19 | 1 | Alpha | "B" = RPI orders available on the buy side; "S" = RPI orders available on the sell side; "A" = RPI orders available on both sides (buy and sell); "N" = No RPI orders available |

---

### 1.8 Direct Listing with Capital Raise Price Discovery Message

The following message is disseminated only for Direct Listing with Capital Raise (DLCR) securities. Nasdaq begins disseminating messages once per second as soon as the DLCR volatility test has successfully passed.

| Name | Offset | Length | Value | Notes |
|------|--------|--------|-------|-------|
| Message Type | 0 | 1 | "O" | Direct Listing with Capital Raise Message |
| Stock Locate | 1 | 2 | Integer | Locate code identifying the security |
| Tracking Number | 3 | 2 | Integer | Nasdaq internal tracking number |
| Timestamp | 5 | 6 | Integer | Nanoseconds since midnight |
| Stock | 11 | 8 | Alpha | Stock symbol, right padded with spaces |
| Open Eligibility Status | 19 | 1 | Alpha | Indicates if the security is eligible to be released for trading. "N": Not Eligible; "Y": Eligible |
| Minimum Allowable Price | 20 | 4 | Price (4) | 20% below Registration Statement Lower Price |
| Maximum Allowable Price | 24 | 4 | Price (4) | 80% above Registration Statement Highest Price |
| Near Execution Price | 28 | 4 | Price (4) | The current reference price when the DLCR volatility test has successfully passed |
| Near Execution Time | 32 | 8 | Integer | The time at which the near execution price was set |
| Lower Price Range Collar | 40 | 4 | Price (4) | Indicates the price of the Lower Auction Collar Threshold (10% below the Near Execution Price) |
| Upper Price Range Collar | 44 | 4 | Price (4) | Indicates the price of the Upper Auction Collar Threshold (10% above the Near Execution Price) |

---

## Support

For general product support and technical support for Nasdaq data feeds, please contact Nasdaq Global Information Services at clientsuccess@nasdaq.com

---

## Appendix A - Documentation and Revision Control Log

### April 28, 2023: Nasdaq TotalView-ITCH Version 5.0
Introduction of Direct Listing with Capital Raise Message and new Imbalance Direction of Paused to NOII Message

### July 14, 2022: Nasdaq TotalView-ITCH Version 5.0
Due to the launch of non-integer leverage factors, updated the Value/Description field of ETP Leverage Factor from section 1.2.1 page 7.

### June 13, 2022: Nasdaq TotalView-ITCH Version 5.0
Corrected subheading number ordering from section 1.2 - 1.5, pages 9 - 18.

### Jan 7, 2022: Nasdaq TotalView-ITCH Version 5.0
Nasdaq added "Extended Trading Close" to the "Cross Type" field under NOII message. For more information, refer http://www.nasdaqtrader.com/TraderNews.aspx?id=dtn2022-1

### May 3, 2018: Nasdaq TotalView-ITCH Version 5.0
Nasdaq made the decision to fall back to the previous version number to avoid customer confusion related to different sequencing between the version number of the product specifications and the version number of the actual product code.

### March 3, 2018: Nasdaq TotalView-ITCH Version 5.0
Released a new version of Nasdaq TotalView-ITCH documentation to add a new Operational Halt message (Section 4.2.8) to indicate the current Operational Status of a security to the trading community.

*(Additional revision history continues... See document for full history)*

---

## Appendix B – Stock Symbol Convention

For Nasdaq-listed issues, Nasdaq currently restricts its symbol length to a maximum of 8 characters. For common stock issuances, Nasdaq, PSX and BX will only assign root symbols of 1 to 4 characters in length with possible fifth and or sixth character denoting a suffix. In certain instances, a dot "." delimiter may be applied to symbols after the root and between the suffix e.g., XXXX.A. For subordinate securities, Nasdaq and BX will assign a 5 character symbol for which the last character relays information about the issue class or issue type. For the current list of fifth and or six character symbol suffixes, please refer to Ticker Symbol Convention page on the Nasdaq Trader website.

For NYSE-, NYSE American- and NYSE Arca-listed securities with subordinate issue types, please refer to Ticker Symbol Convention page on the Nasdaq Trader website.

---

## Appendix C – Trading Action Reason Codes

For Nasdaq-listed issues, Nasdaq acts as the primary market and has the authority to institute a trading halt or trading pause in an issue due to news dissemination or regulatory reasons.

For CQS issues, Nasdaq abides by any regulatory trading halts and trading pauses instituted by the primary or listing market as appropriate.

For both issue types, Nasdaq may also halt trading for operational reasons.

Nasdaq will send out a trading action message to inform its market participants when the trading status of an issue changes. For informational purposes, Nasdaq also attempts to provide the reason for each trading action update. For bandwidth efficiency reasons, Nasdaq uses a 4-byte code for the reason on its outbound data feeds.

### Reason Codes For Trading Halt Actions

| Code | Value |
|------|-------|
| T1 | Halt News Pending |
| T2 | Halt News Disseminated |
| T5 | Single Security Trading Pause In Effect |
| T6 | Regulatory Halt — Extraordinary Market Activity |
| T8 | Halt ETF |
| T12 | Trading Halted; For Information Requested by Listing Market |
| H4 | Halt Non-Compliance |
| H9 | Halt Filings Not Current |
| H10 | Halt SEC Trading Suspension |
| H11 | Halt Regulatory Concern |
| O1 | Operations Halt; Contact Market Operations |
| LUDP | Volatility Trading Pause |
| LUDS | Volatility Trading Pause – Straddle Condition |
| MWC1 | Market Wide Circuit Breaker Halt – Level 1 |
| MWC2 | Market Wide Circuit Breaker Halt – Level 2 |
| MWC3 | Market Wide Circuit Breaker Halt – Level 3 |
| MWC0 | Market Wide Circuit Breaker Halt – Carry over from previous day |
| IPO1 | IPO Issue Not Yet Trading |
| M1 | Corporate Action |
| M2 | Quotation Not Available |
| Space | Reason Not Available |

### Reason Codes for Quotation/Trading Resumption Actions

| Code | Value |
|------|-------|
| T3 | News and Resumption Times |
| T7 | Single Security Trading Pause / Quotation Only Period |
| R4 | Qualifications Issues Reviewed/Resolved; Quotations/Trading to Resume |
| R9 | Filing Requirements Satisfied/Resolved; Quotations/Trading To Resume |
| C3 | Issuer News Not Forthcoming; Quotations/Trading To Resume |
| C4 | Qualifications Halt ended Maintenance Requirements Met; Resume |
| C9 | Qualifications Halt Concluded; Filings Met; Quotes/Trades To Resume |
| C11 | Trade Halt Concluded By Other Regulatory Auth.; Quotes/Trades Resume |
| MWCQ | Market Wide Circuit Breaker Resumption |
| R1 | New Issue Available |
| R2 | Issue Available |
| IPOQ | IPO Security Released for Quotation (Nasdaq Securities Only) |
| IPOE | IPO Security — Positioning Window Extension (Nasdaq Securities Only) |
| Space | Reason Not Available |

---

## Appendix D – Issue Classification Values

Identifies the security class for the issue as assigned by Nasdaq

| Code | Value |
|------|-------|
| A | American Depositary Share |
| B | Bond |
| C | Common Stock |
| F | Depository Receipt |
| I | 144A |
| L | Limited Partnership |
| N | Notes |
| O | Ordinary Share |
| P | Preferred Stock |
| Q | Other Securities |
| R | Right |
| S | Shares of Beneficial Interest |
| T | Convertible Debenture |
| U | Unit |
| V | Units/Benif Int |
| W | Warrant |

---

## Appendix E – Issue Sub Type Values

| Code | Value |
|------|-------|
| A | Preferred Trust Securities |
| AI | Alpha Index ETNs |
| B | Index Based Derivative |
| C | Common Shares |
| CB | Commodity Based Trust Shares |
| CF | Commodity Futures Trust Shares |
| CL | Commodity-Linked Securities |
| CM | Commodity Index Trust Shares |
| CO | Collateralized Mortgage Obligation |
| CT | Currency Trust Shares |
| CU | Commodity-Currency-Linked Securities |
| CW | Currency Warrants |
| D | Global Depositary Shares |
| E | ETF-Portfolio Depositary Receipt |
| EG | Equity Gold Shares |
| EI | ETN-Equity Index-Linked Securities |
| EM | NextShares Exchange Traded Managed Fund* |
| EN | Exchange Traded Notes |
| EU | Equity Units |
| F | HOLDRS |
| FI | ETN-Fixed Income-Linked Securities |
| FL | ETN-Futures-Linked Securities |
| G | Global Shares |
| I | ETF-Index Fund Shares |
| IR | Interest Rate |
| IW | Index Warrant |
| IX | Index-Linked Exchangeable Notes |
| J | Corporate Backed Trust Security |
| L | Contingent Litigation Right |
| LL | Identifies securities of companies that are set up as a Limited Liability Company (LLC) |
| M | Equity-Based Derivative |
| MF | Managed Fund Shares |
| ML | ETN-Multi-Factor Index-Linked Securities |
| MT | Managed Trust Securities |
| N | NY Registry Shares |
| O | Open Ended Mutual Fund |
| P | Privately Held Security |
| PP | Poison Pill |
| PU | Partnership Units |
| Q | Closed-End Funds |
| R | Reg-S |
| RC | Commodity-Redeemable Commodity-Linked Securities |
| RF | ETN-Redeemable Futures-Linked Securities |
| RT | REIT |
| RU | Commodity-Redeemable Currency-Linked Securities |
| S | SEED |
| SC | Spot Rate Closing |
| SI | Spot Rate Intraday |
| T | Tracking Stock |
| TC | Trust Certificates |
| TU | Trust Units |
| U | Portal |
| V | Contingent Value Right |
| W | Trust Issued Receipts |
| WC | World Currency Option |
| X | Trust |
| Y | Other |
| Z | Not Applicable |

*NextShares Exchange Traded Managed Funds (ETMFs) launched in February 2016. NextShares prices are stated in proxy price on this feed. For more information, please refer to the NextShares Homepage.

---

**End of Document**
