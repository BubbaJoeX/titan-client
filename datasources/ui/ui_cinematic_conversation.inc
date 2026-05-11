<Page
	Name='CinematicConversation'
	PackSize='1,1'
	ScrollExtent='1024,768'
	Size='1024,768'
	Visible='false'
	GetsInput='true'
	AbsorbsInput='true'
>
	<Page
		Name='convo'
		PackSize='1,1'
		ScrollExtent='1024,768'
		Size='1024,768'
		GetsInput='true'
		AbsorbsInput='true'
	>
		<Data
			Name='CodeData'
			letterboxTop='letterboxTop'
			letterboxBottom='letterboxBottom'
			dialoguePanel='dialoguePanel'
			npcName='dialoguePanel.dialogueColumn.npcName'
			npcMessage='dialoguePanel.dialogueColumn.npcMessage'
			responseArea='dialoguePanel.dialogueColumn.responseArea'
			responsePanel='dialoguePanel.dialogueColumn.responseArea.responsePanel'
			responseScrollbar='dialoguePanel.dialogueColumn.responseArea.responseScrollbar'
			response1='dialoguePanel.dialogueColumn.responseArea.responsePanel.response1'
			response2='dialoguePanel.dialogueColumn.responseArea.responsePanel.response2'
			response3='dialoguePanel.dialogueColumn.responseArea.responsePanel.response3'
			response4='dialoguePanel.dialogueColumn.responseArea.responsePanel.response4'
			response5='dialoguePanel.dialogueColumn.responseArea.responsePanel.response5'
			response6='dialoguePanel.dialogueColumn.responseArea.responsePanel.response6'
			responsePagePrev='dialoguePanel.dialogueColumn.responseArea.responsePageNav.responsePagePrev'
			responsePageNext='dialoguePanel.dialogueColumn.responseArea.responsePageNav.responsePageNext'
			responsePageLabel='dialoguePanel.dialogueColumn.responseArea.responsePageNav.responsePageLabel'
			endConversation='dialoguePanel.dialogueColumn.endConversation'
			scriptedLookAtCameraHint='dialoguePanel.dialogueColumn.scriptedLookAtCameraHint'
			gmCinematicBadge='gmCinematicBadge'
		/>

		<Page
			BackgroundTint='#000000'
			BackgroundColor='#000000'
			BackgroundOpacity='1.00'
			GetsInput='false'
			AbsorbsInput='false'
			Location='0,0'
			Name='letterboxTop'
			PackLocation='nfn,nfn'
			PackSize='a,f'
			ScrollExtent='1024,0'
			Size='1024,0'
		/>

		<Page
			BackgroundTint='#000000'
			BackgroundColor='#000000'
			BackgroundOpacity='1.00'
			GetsInput='false'
			AbsorbsInput='false'
			Location='0,768'
			Name='letterboxBottom'
			PackLocation='nfn,fff'
			PackSize='a,f'
			ScrollExtent='1024,0'
			Size='1024,0'
		/>

		<Page
			BackgroundColor='#FFFFFF'
			BackgroundTint='#00D6FB'
			PalBgTint='back1'
			BackgroundOpacity='0.52'
			GetsInput='true'
			AbsorbsInput='true'
			Location='0,456'
			Name='dialoguePanel'
			PackLocation='nfn,fff'
			PackSize='a,f'
			RStyleDefault='/Styles.New.fatFrameInside_palW.rs_default'
			ScrollExtent='1024,312'
			Size='1024,312'
		>
			<!-- Fixed-width column centered horizontally on the screen (parent is full width). -->
			<Page
				Name='dialogueColumn'
				PackLocation='cfc,nfn'
				ScrollExtent='952,312'
				Size='952,312'
				GetsInput='true'
				AbsorbsInput='false'
			>
			<Text Font='bold_13' Location='0,12' Name='npcName' PalText='text1' ScrollExtent='952,22' Size='952,22' TextAlignment='Left' TextAlignmentVertical='Center'/>
			<Text Font='bold_12' Location='0,38' Margin='0,0,0,4' Multiline='true' Name='npcMessage' PalText='text1' ScrollExtent='952,74' Size='952,74' TextAlignment='Left' TextAlignmentVertical='Top' Wrap='true'/>

			<Page Location='0,114' Name='dialogueDivider' PackSize='a,f' ScrollExtent='952,4' Size='952,4' BackgroundOpacity='0.75' BackgroundTint='#00D6FB' GetsInput='false' AbsorbsInput='false' PalBgTint='line1'/>

			<!-- Response rows: full width, text centered; scrollbar at right edge -->
			<Page Location='0,122' Name='responseArea' ScrollExtent='952,158' Size='952,158' GetsInput='true' AbsorbsInput='false'>
				<Page Location='0,0' Name='responsePanel' GetsInput='true' ScrollExtent='952,92' Size='952,92'>
				<Page Location='0,0' Name='response1' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Center' TextAlignmentVertical='Center' TextColor='#000000' TextColorHighlight='#000000'/>
					<Text Name='prefix' Font='bold_11' GetsInput='false' Location='8,1' ScrollExtent='120,20' Size='120,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='highlight' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='132,1' ScrollExtent='812,20' Size='812,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1' Visible='false'/>
				</Page>
				<Page Location='0,23' Name='response2' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Center' TextAlignmentVertical='Center' TextColor='#000000' TextColorHighlight='#000000'/>
					<Text Name='prefix' Font='bold_11' GetsInput='false' Location='8,1' ScrollExtent='120,20' Size='120,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='highlight' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='132,1' ScrollExtent='812,20' Size='812,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1' Visible='false'/>
				</Page>
				<Page Location='0,46' Name='response3' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Center' TextAlignmentVertical='Center' TextColor='#000000' TextColorHighlight='#000000'/>
					<Text Name='prefix' Font='bold_11' GetsInput='false' Location='8,1' ScrollExtent='120,20' Size='120,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='highlight' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='132,1' ScrollExtent='812,20' Size='812,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1' Visible='false'/>
				</Page>
				<Page Location='0,69' Name='response4' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Center' TextAlignmentVertical='Center' TextColor='#000000' TextColorHighlight='#000000'/>
					<Text Name='prefix' Font='bold_11' GetsInput='false' Location='8,1' ScrollExtent='120,20' Size='120,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='highlight' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='132,1' ScrollExtent='812,20' Size='812,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1' Visible='false'/>
				</Page>
				<Page Location='0,92' Name='response5' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Center' TextAlignmentVertical='Center' TextColor='#000000' TextColorHighlight='#000000'/>
					<Text Name='prefix' Font='bold_11' GetsInput='false' Location='8,1' ScrollExtent='120,20' Size='120,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='highlight' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='132,1' ScrollExtent='812,20' Size='812,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1' Visible='false'/>
				</Page>
				<Page Location='0,115' Name='response6' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Center' TextAlignmentVertical='Center' TextColor='#000000' TextColorHighlight='#000000'/>
					<Text Name='prefix' Font='bold_11' GetsInput='false' Location='8,1' ScrollExtent='120,20' Size='120,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='highlight' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='132,1' ScrollExtent='812,20' Size='812,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1' Visible='false'/>
				</Page>
				</Page>
				<Scrollbar
					Name='responseScrollbar'
					Control='responsePanel'
					Location='940,0'
					ScrollExtent='12,92'
					Size='12,92'
					Style='/Styles.scrollbar.default.vHud4.style'
					AutoVisibility='true'
				/>
				<Page Location='0,94' Name='responsePageNav' ScrollExtent='952,22' Size='952,22' GetsInput='true' AbsorbsInput='false'>
					<Button Location='0,0' Name='responsePagePrev' ScrollExtent='96,22' Size='96,22' Style='/Styles.New.buttons.hud.style' Text='Prev' TextAlignment='Center' TextAlignmentVertical='Center' Visible='false'/>
					<Text Font='bold_11' Location='200,2' Name='responsePageLabel' ScrollExtent='552,18' Size='552,18' PalText='text1' TextAlignment='Center' TextAlignmentVertical='Center' Visible='false'/>
					<Button Location='856,0' Name='responsePageNext' ScrollExtent='96,22' Size='96,22' Style='/Styles.New.buttons.hud.style' Text='Next' TextAlignment='Center' TextAlignmentVertical='Center' Visible='false'/>
				</Page>
			</Page>

			<Button
				Location='378,246'
				Name='endConversation'
				ScrollExtent='196,28'
				Size='196,28'
				IsCancelButton='true'
				Style='/Styles.New.buttons.hud.style'
				Text='End Conversation'
				TextAlignment='Center'
				PalText='text1'
				TextColor='#000000'
				TextAlignmentVertical='Center'
			/>
			<Text Font='bold_10' GetsInput='false' Location='0,280' Name='scriptedLookAtCameraHint' PalText='highlight' ScrollExtent='952,14' Size='952,14' TextAlignment='Center' TextAlignmentVertical='Center' Visible='false'/>
			</Page>
		</Page>
		<Text Font='bold_10' GetsInput='false' PackLocation='fff,nfn' Name='gmCinematicBadge' ScrollExtent='44,16' Size='44,16' Text='*GM*' TextAlignment='Right' TextAlignmentVertical='Center' TextColor='#FF2222' Visible='false' Margin='8,8,8,0'/>
	</Page>
</Page>
