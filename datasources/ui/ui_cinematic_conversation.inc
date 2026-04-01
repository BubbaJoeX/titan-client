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
			npcName='dialoguePanel.npcName'
			npcMessage='dialoguePanel.npcMessage'
			responsePanel='dialoguePanel.responsePanel'
			response1='dialoguePanel.responsePanel.response1'
			response2='dialoguePanel.responsePanel.response2'
			response3='dialoguePanel.responsePanel.response3'
			response4='dialoguePanel.responsePanel.response4'
			response5='dialoguePanel.responsePanel.response5'
			response6='dialoguePanel.responsePanel.response6'
			endConversation='dialoguePanel.endConversation'
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
			BackgroundColor='#0d0d12'
			BackgroundOpacity='1.00'
			GetsInput='true'
			AbsorbsInput='true'
			Location='0,486'
			Name='dialoguePanel'
			PackLocation='nfn,fff'
			PackSize='a,f'
			RStyleDefault='/Styles.New.fatFrameInside_palW.rs_default'
			ScrollExtent='1024,282'
			Size='1024,282'
		>
			<Text Font='bold_12' Location='36,14' Name='npcName' PalText='header' ScrollExtent='952,20' Size='952,20' TextColor='#FFCC55' TextAlignment='Left' TextAlignmentVertical='Center'/>
			<Text Font='bold_12' Location='36,40' Multiline='true' Name='npcMessage' PalText='text1' ScrollExtent='952,70' Size='952,70' TextAlignment='Left' TextAlignmentVertical='Top' TextColor='#FFFFFF' Wrap='true'/>

			<Page Location='36,118' Name='responsePanel' GetsInput='true' ScrollExtent='952,138' Size='952,138'>
				<Page Location='0,0' Name='response1' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#FFFFFF' TextColorHighlight='#FFEB9A'/>
					<Text Name='prefix' GetsInput='false' Location='10,1' ScrollExtent='138,20' Size='138,20' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#6CD6FF' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='152,1' ScrollExtent='792,20' Size='792,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1'/>
				</Page>
				<Page Location='0,23' Name='response2' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#FFFFFF' TextColorHighlight='#FFEB9A'/>
					<Text Name='prefix' GetsInput='false' Location='10,1' ScrollExtent='138,20' Size='138,20' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#6CD6FF' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='152,1' ScrollExtent='792,20' Size='792,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1'/>
				</Page>
				<Page Location='0,46' Name='response3' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#FFFFFF' TextColorHighlight='#FFEB9A'/>
					<Text Name='prefix' GetsInput='false' Location='10,1' ScrollExtent='138,20' Size='138,20' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#6CD6FF' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='152,1' ScrollExtent='792,20' Size='792,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1'/>
				</Page>
				<Page Location='0,69' Name='response4' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#FFFFFF' TextColorHighlight='#FFEB9A'/>
					<Text Name='prefix' GetsInput='false' Location='10,1' ScrollExtent='138,20' Size='138,20' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#6CD6FF' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='152,1' ScrollExtent='792,20' Size='792,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1'/>
				</Page>
				<Page Location='0,92' Name='response5' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#FFFFFF' TextColorHighlight='#FFEB9A'/>
					<Text Name='prefix' GetsInput='false' Location='10,1' ScrollExtent='138,20' Size='138,20' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#6CD6FF' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='152,1' ScrollExtent='792,20' Size='792,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1'/>
				</Page>
				<Page Location='0,115' Name='response6' ScrollExtent='952,22' Size='952,22' Visible='false'>
					<Button Name='button' Location='0,0' ScrollExtent='952,22' Size='952,22' Style='/Styles.New.buttons.hud.style' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#FFFFFF' TextColorHighlight='#FFEB9A'/>
					<Text Name='prefix' GetsInput='false' Location='10,1' ScrollExtent='138,20' Size='138,20' TextAlignment='Left' TextAlignmentVertical='Center' TextColor='#6CD6FF' Visible='false'/>
					<Text Name='text' GetsInput='false' Location='152,1' ScrollExtent='792,20' Size='792,20' TextAlignment='Left' TextAlignmentVertical='Center' PalText='text1'/>
				</Page>
			</Page>

			<Button
				Location='792,246'
				Name='endConversation'
				ScrollExtent='196,28'
				Size='196,28'
				IsCancelButton='true'
				Style='/Styles.New.buttons.hud.style'
				Text='End Conversation'
				TextAlignment='Center'
				PalText='header'
				TextAlignmentVertical='Center'
			/>
		</Page>
	</Page>
</Page>
